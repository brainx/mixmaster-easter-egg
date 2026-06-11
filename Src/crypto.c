/* Mixmaster version 3.0  --  (C) 1999 - 2006 Anonymizer Inc. and others.

   Mixmaster may be redistributed and modified under certain conditions.
   This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
   ANY KIND, either express or implied. See the file COPYRIGHT for
   details.

   Interface to cryptographic library
   MODERNIZED-2026  see MODERNIZATION.md
   $Id: crypto.c 934 2006-06-24 13:40:39Z rabbi $ */


#include "mix3.h"
#include "crypto.h"
#include <assert.h>
#include <string.h>
#include <time.h>

#ifdef USE_OPENSSL
int digestmem_md5(byte *b, int n, BUFFER *md)
{
  byte m[MD5_DIGEST_LENGTH];

  MD5(b, n, m);
  buf_reset(md);
  buf_append(md, m, MD5_DIGEST_LENGTH);
  return (0);
}

int digest_md5(BUFFER *b, BUFFER *md)
{
  return (digestmem_md5(b->data, b->length, md));
}

int isdigest_md5(BUFFER *b, BUFFER *md)
{
  int ret;
  BUFFER *newmd;

  newmd = buf_new();
  digest_md5(b, newmd);
  ret = buf_eq(md, newmd);
  buf_free(newmd);
  return (ret);
}

static int digestmem_sha1(byte *b, int n, BUFFER *md)
{
  byte m[SHA_DIGEST_LENGTH];

  SHA1(b, n, m);
  buf_reset(md);
  buf_append(md, m, SHA_DIGEST_LENGTH);
  return (0);
}

int digest_sha1(BUFFER *b, BUFFER *md)
{
  return (digestmem_sha1(b->data, b->length, md));
}

static int digestmem_rmd160(byte *b, int n, BUFFER *md)
{
  byte m[RIPEMD160_DIGEST_LENGTH];

  RIPEMD160(b, n, m);
  buf_reset(md);
  buf_append(md, m, RIPEMD160_DIGEST_LENGTH);
  return (0);
}

int digest_rmd160(BUFFER *b, BUFFER *md)
{
  return (digestmem_rmd160(b->data, b->length, md));
}

#define MAX_RSA_MODULUS_LEN 128

static int read_seckey(BUFFER *buf, SECKEY *key, const byte id[])
{
  BUFFER *md;
  int bits;
  int len, plen;
  byte *ptr;
  int err = 0;

  md = buf_new();
  bits = buf->data[0] + 256 * buf->data[1];
  len = (bits + 7) / 8;
  plen = (len + 1) / 2;

  /* due to encryption, buffer size is multiple of 8 */
  if (3 * len + 5 * plen + 8 < buf->length || 3 * len + 5 * plen > buf->length)
    return (-1);

  {
    BIGNUM *n, *e, *d, *p, *q, *dmp1, *dmq1, *iqmp;

    ptr = buf->data + 2;

    n = BN_bin2bn(ptr, len, NULL);
    buf_append(md, ptr, len);
    ptr += len;

    e = BN_bin2bn(ptr, len, NULL);
    buf_append(md, ptr, len);
    ptr += len;

    d = BN_bin2bn(ptr, len, NULL);
    ptr += len;

    p = BN_bin2bn(ptr, plen, NULL);
    ptr += plen;

    q = BN_bin2bn(ptr, plen, NULL);
    ptr += plen;

    dmp1 = BN_bin2bn(ptr, plen, NULL);
    ptr += plen;

    dmq1 = BN_bin2bn(ptr, plen, NULL);
    ptr += plen;

    iqmp = BN_bin2bn(ptr, plen, NULL);
    ptr += plen;

    if (!RSA_set0_key(key, n, e, d) ||
	!RSA_set0_factors(key, p, q) ||
	!RSA_set0_crt_params(key, dmp1, dmq1, iqmp)) {
      BN_free(n); BN_free(e); BN_free(d);
      BN_free(p); BN_free(q);
      BN_free(dmp1); BN_free(dmq1); BN_free(iqmp);
      buf_free(md);
      return (-1);
    }
  }

  digest_md5(md, md);
  if (id)
    err = (memcmp(id, md->data, 16) == 0) ? 0 : -1;
  buf_free(md);
  return (err);
}

static int read_pubkey(BUFFER *buf, PUBKEY *key, const byte id[])
{
  BUFFER *md;
  int bits;
  int len;
  byte *ptr;
  int err = 0;

  md = buf_new();
  bits = buf->data[0] + 256 * buf->data[1];
  len = (bits + 7) / 8;

  if (2 * len + 2 != buf->length)
    return (-1);

  {
    BIGNUM *n, *e;

    ptr = buf->data + 2;

    n = BN_bin2bn(ptr, len, NULL);
    buf_append(md, ptr, len);
    ptr += len;

    e = BN_bin2bn(ptr, len, NULL);
    buf_append(md, ptr, len);
    ptr += len;

    if (!RSA_set0_key(key, n, e, NULL)) {
      BN_free(n);
      BN_free(e);
      buf_free(md);
      return (-1);
    }
  }

  digest_md5(md, md);
  if (id)
    err = (memcmp(id, md->data, 16) == 0) ? 0 : -1;
  buf_free(md);
  return (err);
}

static int write_seckey(BUFFER *sk, SECKEY *key, byte keyid[])
{
  byte l[128];
  int n;
  BUFFER *b, *temp;
  const BIGNUM *bn, *be, *bd, *bp, *bq, *bdmp1, *bdmq1, *biqmp;

  b = buf_new();
  temp = buf_new();

  RSA_get0_key(key, &bn, &be, &bd);
  RSA_get0_factors(key, &bp, &bq);
  RSA_get0_crt_params(key, &bdmp1, &bdmq1, &biqmp);

  n = BN_bn2bin(bn, l);
  assert(n <= 128);
  if (n < 128)
    buf_appendzero(b, 128 - n);
  buf_append(b, l, n);

  n = BN_bn2bin(be, l);
  assert(n <= 128);
  if (n < 128)
    buf_appendzero(b, 128 - n);
  buf_append(b, l, n);

  digest_md5(b, temp);
  memcpy(keyid, temp->data, 16);

  buf_appendc(sk, 0);
  buf_appendc(sk, 4);
  buf_cat(sk, b);

  n = BN_bn2bin(bd, l);
  assert(n <= 128);
  if (n < 128)
    buf_appendzero(sk, 128 - n);
  buf_append(sk, l, n);

  n = BN_bn2bin(bp, l);
  assert(n <= 64);
  if (n < 64)
    buf_appendzero(sk, 64 - n);
  buf_append(sk, l, n);

  n = BN_bn2bin(bq, l);
  assert(n <= 64);
  if (n < 64)
    buf_appendzero(sk, 64 - n);
  buf_append(sk, l, n);

  n = BN_bn2bin(bdmp1, l);
  assert(n <= 64);
  if (n < 64)
    buf_appendzero(sk, 64 - n);
  buf_append(sk, l, n);

  n = BN_bn2bin(bdmq1, l);
  assert(n <= 64);
  if (n < 64)
    buf_appendzero(sk, 64 - n);
  buf_append(sk, l, n);

  n = BN_bn2bin(biqmp, l);
  assert(n <= 64);
  if (n < 64)
    buf_appendzero(sk, 64 - n);
  buf_append(sk, l, n);

  buf_pad(sk, 712);		/* encrypt needs a block size multiple of 8 */

  buf_free(temp);
  buf_free(b);
  return (0);
}

static int write_pubkey(BUFFER *pk, PUBKEY *key, byte keyid[])
{
  byte l[128];
  int n;
  const BIGNUM *bn, *be;

  (void)keyid;
  RSA_get0_key(key, &bn, &be, NULL);

  buf_appendc(pk, 0);
  buf_appendc(pk, 4);
  n = BN_bn2bin(bn, l);
  assert(n <= 128);
  if (n < 128)
    buf_appendzero(pk, 128 - n);
  buf_append(pk, l, n);
  n = BN_bn2bin(be, l);
  assert(n <= 128);
  if (n < 128)
    buf_appendzero(pk, 128 - n);
  buf_append(pk, l, n);
  return (0);
}

int seckeytopub(BUFFER *pub, BUFFER *sec, byte keyid[])
{
  RSA *k;
  int err = 0;

  k = RSA_new();
  err = read_seckey(sec, k, keyid);
  if (err == 0)
    err = write_pubkey(pub, k, keyid);
  RSA_free(k);
  return (err);
}

int check_pubkey(BUFFER *buf, const byte id[])
{
  RSA *tmp;
  int ret;

  tmp = RSA_new();
  ret = read_pubkey(buf, tmp, id);
  RSA_free(tmp);
  return (ret);
}

int check_seckey(BUFFER *buf, const byte id[])
{
  RSA *tmp;
  int ret;

  tmp = RSA_new();
  ret = read_seckey(buf, tmp, id);
  RSA_free(tmp);
  return (ret);
}

int v2createkey(void)
{
  RSA *k = NULL;
  BUFFER *b, *ek, *iv;
  int err = 0;
  FILE *f;
  byte keyid[16];
  char line[33];

  b = buf_new();
  ek = buf_new();
  iv = buf_new();

  {
    BIGNUM *e;

    errlog(NOTICE, "Generating RSA key.\n");
    e = BN_new();
    BN_set_word(e, 65537);
    k = RSA_new();
    if (k == NULL || RSA_generate_key_ex(k, 1024, e, NULL) != 1) {
      BN_free(e);
      RSA_free(k);
      k = NULL;
      err = -1;
      goto keygen_done;
    }
    BN_free(e);
  }
  err = write_seckey(b, k, keyid);
keygen_done:
  RSA_free(k);
  if (err == 0) {
    f = mix_openfile(SECRING, "a");
    if (f != NULL) {
      time_t now = time(NULL);
      struct tm *gt;
      gt = gmtime(&now);
      strftime(line, LINELEN, "%Y-%m-%d", gt);
      fprintf(f, "%s\nCreated: %s\n", begin_key, line);
      if (KEYLIFETIME) {
	now += KEYLIFETIME;
	gt = gmtime(&now);
	strftime(line, LINELEN, "%Y-%m-%d", gt);
	fprintf(f, "Expires: %s\n", line);
      }
      id_encode(keyid, line);
      buf_appends(ek, PASSPHRASE);
      digest_md5(ek, ek);
      buf_setrnd(iv, 8);
      buf_crypt(b, ek, iv, ENCRYPT);
      encode(b, 40);
      encode(iv, 0);
      fprintf(f, "%s\n0\n%s\n", line, iv->data);
      buf_write(b, f);
      fprintf(f, "%s\n\n", end_key);
      fclose(f);
    } else
      err = -1;
  }
  if (err != 0)
    errlog(ERRORMSG, "Key generation failed.\n");

  buf_free(b);
  buf_free(ek);
  buf_free(iv);
  return (err);
}

int pk_decrypt(BUFFER *in, BUFFER *keybuf)
{
  int err = 0;
  BUFFER *out;
  RSA *key;

  out = buf_new();
  key = RSA_new();
  read_seckey(keybuf, key, NULL);

  buf_prepare(out, in->length);
  out->length = RSA_private_decrypt(in->length, in->data, out->data, key,
				    RSA_PKCS1_PADDING);
  if (out->length == -1)
    err = -1, out->length = 0;

  RSA_free(key);
  buf_move(in, out);
  buf_free(out);
  return (err);
}

int pk_encrypt(BUFFER *in, BUFFER *keybuf)
{
  BUFFER *out;
  RSA *key;
  int err = 0;

  out = buf_new();
  key = RSA_new();
  read_pubkey(keybuf, key, NULL);

  buf_prepare(out, RSA_size(key));
  out->length = RSA_public_encrypt(in->length, in->data, out->data, key,
				   RSA_PKCS1_PADDING);
  if (out->length == -1)
    out->length = 0, err = -1;
  buf_move(in, out);
  buf_free(out);
  RSA_free(key);
  return (err);
}
int buf_crypt(BUFFER *buf, BUFFER *key, BUFFER *iv, int enc)
{
  des_key_schedule ks1;
  des_key_schedule ks2;
  des_key_schedule ks3;
  des_cblock i;

  assert(enc == ENCRYPT || enc == DECRYPT);
  assert((key->length == 16 || key->length == 24) && iv->length == 8);
  assert(buf->length % 8 == 0);

  memcpy(i, iv->data, 8);	/* leave iv buffer unchanged */
  des_set_key((const_des_cblock *) key->data, &ks1);
  des_set_key((const_des_cblock *) (key->data + 8), &ks2);
  if (key->length == 16)
    des_set_key((const_des_cblock *) key->data, &ks3);
  else
    des_set_key((const_des_cblock *) (key->data + 16), &ks3);
  des_ede3_cbc_encrypt(buf->data, buf->data, buf->length, &ks1, &ks2, &ks3,
		       &i, enc);
  return (0);
}

int buf_3descrypt(BUFFER *buf, BUFFER *key, BUFFER *iv, int enc)
{
  int n = 0;
  des_key_schedule ks1;
  des_key_schedule ks2;
  des_key_schedule ks3;

  assert(enc == ENCRYPT || enc == DECRYPT);
  assert(key->length == 24 && iv->length == 8);

  des_set_key((const_des_cblock *) key->data, &ks1);
  des_set_key((const_des_cblock *) (key->data + 8), &ks2);
  des_set_key((const_des_cblock *) (key->data + 16), &ks3);
  des_ede3_cfb64_encrypt(buf->data, buf->data, buf->length, &ks1, &ks2, &ks3,
			(des_cblock *) iv->data, &n, enc);
  return (0);
}

int buf_bfcrypt(BUFFER *buf, BUFFER *key, BUFFER *iv, int enc)
{
  int n = 0;
  BF_KEY ks;

  if (key == NULL || key->length == 0)
    return (-1);

  assert(enc == ENCRYPT || enc == DECRYPT);
  assert(key->length == 16 && iv->length == 8);
  BF_set_key(&ks, key->length, key->data);
  BF_cfb64_encrypt(buf->data, buf->data, buf->length, &ks, iv->data, &n,
		     enc == ENCRYPT ? BF_ENCRYPT : BF_DECRYPT);
  return (0);
}

int buf_castcrypt(BUFFER *buf, BUFFER *key, BUFFER *iv, int enc)
{
  int n = 0;
  CAST_KEY ks;

  if (key == NULL || key->length == 0)
    return (-1);

  assert(enc == ENCRYPT || enc == DECRYPT);
  assert(key->length == 16 && iv->length == 8);
  CAST_set_key(&ks, 16, key->data);
  CAST_cfb64_encrypt(buf->data, buf->data, buf->length, &ks, iv->data, &n,
		     enc == ENCRYPT ? CAST_ENCRYPT : CAST_DECRYPT);
  return (0);
}

#ifdef USE_AES
int buf_aescrypt(BUFFER *buf, BUFFER *key, BUFFER *iv, int enc)
{
  int n = 0;
  AES_KEY ks;

  if (key == NULL || key->length == 0)
    return (-1);

  assert(enc == ENCRYPT || enc == DECRYPT);
  assert((key->length == 16 || key->length == 24 || key->length == 32) && iv->length == 16);
  AES_set_encrypt_key(key->data, key->length<<3, &ks);
  AES_cfb128_encrypt(buf->data, buf->data, buf->length, &ks, iv->data, &n,
		     enc == ENCRYPT ? AES_ENCRYPT : AES_DECRYPT);
  return (0);
}
#endif /* USE_AES */

#ifdef USE_IDEA
int buf_ideacrypt(BUFFER *buf, BUFFER *key, BUFFER *iv, int enc)
{
  int n = 0;
  IDEA_KEY_SCHEDULE ks;

  if (key == NULL || key->length == 0)
    return (-1);

  assert(enc == ENCRYPT || enc == DECRYPT);
  assert(key->length == 16 && iv->length == 8);
  idea_set_encrypt_key(key->data, &ks);
  idea_cfb64_encrypt(buf->data, buf->data, buf->length, &ks, iv->data, &n,
		     enc == ENCRYPT ? IDEA_ENCRYPT : IDEA_DECRYPT);
  return (0);
}
#endif /* USE_IDEA */
#endif /* USE_OPENSSL */
