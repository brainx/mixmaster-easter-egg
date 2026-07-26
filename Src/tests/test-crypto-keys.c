/* Mixmaster 3.0 — regression test for the v2 RSA key readers (NEW-2026)
   Links the real Src/crypto.c; see MODERNIZATION.md.

   Covers:
     - a well-formed 1024-bit key round-trips through read_seckey/read_pubkey
     - seckeytopub() yields the canonical 258-byte public blob
     - pk_encrypt/pk_decrypt round-trip actual plaintext
     - malformed and truncated blobs are rejected without reading out of bounds
       (run under -fsanitize=address to check the second half of that claim)

   $Id: test-crypto-keys.c $ */

#include "../mix3.h"
#include "../crypto.h"
#include <stdio.h>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

static int failures = 0;

static void ok(int cond, const char *what)
{
  printf("%s: %s\n", cond ? "ok" : "FAIL", what);
  if (!cond)
    failures++;
}

/* Serialize an RSA key the way write_seckey() does: a 2-byte little-endian bit
   count, then n and e zero-padded to 128, then d padded to 128, then the five
   CRT values padded to 64 each, then padded out to a multiple of 8. */
static void append_bn(BUFFER *b, const BIGNUM *bn, int width)
{
  byte tmp[128];
  int n = BN_bn2bin(bn, tmp);

  if (n < width)
    buf_appendzero(b, width - n);
  buf_append(b, tmp, n);
}

/* Also reports the key id, which is MD5 over the padded n||e -- the readers take
   it as the fingerprint to verify against, so callers must know it up front. */
static void build_seckey(BUFFER *sec, RSA *k, byte keyid[16])
{
  const BIGNUM *n, *e, *d, *p, *q, *dmp1, *dmq1, *iqmp;
  BUFFER *ne, *md;

  RSA_get0_key(k, &n, &e, &d);
  RSA_get0_factors(k, &p, &q);
  RSA_get0_crt_params(k, &dmp1, &dmq1, &iqmp);

  ne = buf_new();
  md = buf_new();
  append_bn(ne, n, 128);
  append_bn(ne, e, 128);
  digest_md5(ne, md);
  memcpy(keyid, md->data, 16);

  buf_clear(sec);
  buf_appendc(sec, 0);
  buf_appendc(sec, 4);		/* 0 + 256*4 = 1024 bits */
  buf_cat(sec, ne);
  append_bn(sec, d, 128);
  append_bn(sec, p, 64);
  append_bn(sec, q, 64);
  append_bn(sec, dmp1, 64);
  append_bn(sec, dmq1, 64);
  append_bn(sec, iqmp, 64);
  buf_pad(sec, 712);

  buf_free(ne);
  buf_free(md);
}

int main(void)
{
  RSA *k;
  BIGNUM *e;
  BUFFER *sec, *pub, *msg, *cipher, *bad;
  byte keyid[16];

  /* crypto.c draws randomness through rnd_bytes(), which aborts unless the PRNG
     has been declared seeded. OpenSSL 3 self-seeds from the OS. */
  rnd_initialized();

  sec = buf_new();
  pub = buf_new();
  msg = buf_new();
  cipher = buf_new();
  bad = buf_new();

  e = BN_new();
  BN_set_word(e, 65537);
  k = RSA_new();
  if (k == NULL || RSA_generate_key_ex(k, 1024, e, NULL) != 1) {
    printf("FAIL: could not generate a 1024-bit RSA key\n");
    return (1);
  }
  BN_free(e);

  build_seckey(sec, k, keyid);
  ok(sec->length == 712, "secret key blob is 712 bytes");

  /* read_seckey() via check_seckey(): NULL id skips the fingerprint compare, so
     0 here means the blob parsed and the length checks accepted it. */
  ok(check_seckey(sec, NULL) == 0, "well-formed secret key is accepted");

  /* keyid is an input: the readers compare it against MD5(n||e) */
  ok(check_seckey(sec, keyid) == 0, "secret key matches its own key id");

  ok(seckeytopub(pub, sec, keyid) == 0, "seckeytopub() succeeds");
  ok(pub->length == 258, "public key blob is 258 bytes (2*128 + 2)");
  ok(check_pubkey(pub, NULL) == 0, "well-formed public key is accepted");
  ok(check_pubkey(pub, keyid) == 0, "public key matches that key id");

  {
    byte wrong[16];

    memcpy(wrong, keyid, 16);
    wrong[0] ^= 0xff;
    ok(check_seckey(sec, wrong) == -1, "secret key rejects a mismatched key id");
    ok(check_pubkey(pub, wrong) == -1, "public key rejects a mismatched key id");
  }

  /* full asymmetric round trip through both readers */
  buf_sets(msg, "mixmaster type II session key payload");
  buf_set(cipher, msg);
  ok(pk_encrypt(cipher, pub) == 0, "pk_encrypt() succeeds");
  ok(cipher->length == 128, "ciphertext is one 1024-bit block");
  ok(pk_decrypt(cipher, sec) == 0, "pk_decrypt() succeeds");
  ok(buf_eq(cipher, msg), "decrypted plaintext matches the original");

  /* Negative cases. Each of these must be rejected rather than parsed; under
     ASan they also assert that nothing reads past the end of the buffer. */
  buf_clear(bad);
  ok(check_seckey(bad, NULL) == -1, "empty buffer rejected as secret key");
  ok(check_pubkey(bad, NULL) == -1, "empty buffer rejected as public key");

  buf_clear(bad);
  buf_appendc(bad, 0);
  ok(check_seckey(bad, NULL) == -1, "1-byte buffer rejected as secret key");
  ok(check_pubkey(bad, NULL) == -1, "1-byte buffer rejected as public key");

  /* declares 1024 bits but carries no key material */
  buf_clear(bad);
  buf_appendc(bad, 0);
  buf_appendc(bad, 4);
  ok(check_seckey(bad, NULL) == -1, "truncated secret key rejected");
  ok(check_pubkey(bad, NULL) == -1, "truncated public key rejected");

  /* declares 65535 bits: len would be 8192, well past MAX_RSA_MODULUS_LEN */
  buf_clear(bad);
  buf_appendc(bad, 255);
  buf_appendc(bad, 255);
  buf_appendzero(bad, 710);
  ok(check_seckey(bad, NULL) == -1, "oversized modulus rejected as secret key");
  ok(check_pubkey(bad, NULL) == -1, "oversized modulus rejected as public key");

  /* right declared size, one byte short of the required span */
  buf_clear(bad);
  buf_appendc(bad, 0);
  buf_appendc(bad, 4);
  buf_appendzero(bad, 703);	/* 705 total; read_seckey needs >= 706 */
  ok(check_seckey(bad, NULL) == -1, "secret key one byte short rejected");

  buf_clear(bad);
  buf_appendc(bad, 0);
  buf_appendc(bad, 4);
  buf_appendzero(bad, 255);	/* 257 total; read_pubkey needs exactly 258 */
  ok(check_pubkey(bad, NULL) == -1, "public key one byte short rejected");

  /* pk_encrypt/pk_decrypt used to discard the key-parse result and then hand an
     unpopulated RSA object to OpenSSL, which dereferenced a NULL BIGNUM. These
     two must return an error rather than crash. */
  buf_clear(bad);
  buf_sets(msg, "payload");
  buf_set(cipher, msg);
  ok(pk_encrypt(cipher, bad) == -1, "pk_encrypt rejects a malformed public key");

  buf_clear(bad);
  buf_set(cipher, msg);
  ok(pk_decrypt(cipher, bad) == -1, "pk_decrypt rejects a malformed secret key");

  /* a plausible-looking but corrupt 258-byte public blob: right length, garbage
     modulus. must not be used to encrypt. */
  buf_clear(bad);
  buf_appendc(bad, 0);
  buf_appendc(bad, 4);
  buf_appendzero(bad, 256);
  buf_set(cipher, msg);
  ok(pk_encrypt(cipher, bad) == -1, "pk_encrypt rejects a zero modulus");

  RSA_free(k);
  buf_free(sec);
  buf_free(pub);
  buf_free(msg);
  buf_free(cipher);
  buf_free(bad);

  if (failures) {
    printf("\n%d check(s) failed.\n", failures);
    return (1);
  }
  printf("\nAll checks passed.\n");
  return (0);
}
