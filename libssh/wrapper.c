/*
 * wrapper.c - wrapper for crytpo functions
 *
 * This file is part of the SSH Library
 *
 * Copyright (c) 2003      by Aris Adamantiadis
 *
 * The SSH Library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * The SSH Library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the SSH Library; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * vim: ts=2 sw=2 et cindent
 */

/*
 * Why a wrapper?
 *
 * Let's say you want to port libssh from libcrypto of openssl to libfoo
 * you are going to spend hours to remove every references to SHA1_Update()
 * to libfoo_sha1_update after the work is finished, you're going to have
 * only this file to modify it's not needed to say that your modifications
 * are welcome.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libssh/priv.h"

#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>

#include "libssh/crypto.h"

static int alloc_key(struct crypto_struct *cipher) {
    cipher->key = malloc(cipher->keylen);
    if (cipher->key == NULL) {
      return -1;
    }

    return 0;
}

SHACTX sha1_init(void) {
  SHACTX ctx = NULL;
  gcry_md_open(&ctx, GCRY_MD_SHA1, 0);

  return ctx;
}

void sha1_update(SHACTX c, const void *data, unsigned long len) {
  gcry_md_write(c, data, len);
}

void sha1_final(unsigned char *md, SHACTX c) {
  gcry_md_final(c);
  memcpy(md, gcry_md_read(c, 0), SHA_DIGEST_LEN);
  gcry_md_close(c);
}

void sha1(unsigned char *digest, int len, unsigned char *hash) {
  gcry_md_hash_buffer(GCRY_MD_SHA1, hash, digest, len);
}

MD5CTX md5_init(void) {
  MD5CTX c = NULL;
  gcry_md_open(&c, GCRY_MD_MD5, 0);

  return c;
}

void md5_update(MD5CTX c, const void *data, unsigned long len) {
    gcry_md_write(c,data,len);
}

void md5_final(unsigned char *md, MD5CTX c) {
  gcry_md_final(c);
  memcpy(md, gcry_md_read(c, 0), MD5_DIGEST_LEN);
  gcry_md_close(c);
}

HMACCTX hmac_init(const void *key, int len, int type) {
  HMACCTX c = NULL;

  switch(type) {
    case HMAC_SHA1:
      gcry_md_open(&c, GCRY_MD_SHA1, GCRY_MD_FLAG_HMAC);
      break;
    case HMAC_MD5:
      gcry_md_open(&c, GCRY_MD_MD5, GCRY_MD_FLAG_HMAC);
      break;
    default:
      c = NULL;
  }

  gcry_md_setkey(c, key, len);

  return c;
}

void hmac_update(HMACCTX c, const void *data, unsigned long len) {
  gcry_md_write(c, data, len);
}

void hmac_final(HMACCTX c, unsigned char *hashmacbuf, unsigned int *len) {
  *len = gcry_md_get_algo_dlen(gcry_md_get_algo(c));
  memcpy(hashmacbuf, gcry_md_read(c, 0), *len);
  gcry_md_close(c);
}

/* the wrapper functions for blowfish */
static int blowfish_set_key(struct crypto_struct *cipher, void *key, void *IV){
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }

    if (gcry_cipher_open(&cipher->key[0], GCRY_CIPHER_BLOWFISH,
        GCRY_CIPHER_MODE_CBC, 0)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setkey(cipher->key[0], key, 16)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setiv(cipher->key[0], IV, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
  }

  return 0;
}

static void blowfish_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len) {
  gcry_cipher_encrypt(cipher->key[0], out, len, in, len);
}

static void blowfish_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len) {
  gcry_cipher_decrypt(cipher->key[0], out, len, in, len);
}

static int aes_set_key(struct crypto_struct *cipher, void *key, void *IV) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }

    switch (cipher->keysize) {
      case 128:
        if (gcry_cipher_open(&cipher->key[0], GCRY_CIPHER_AES128,
              GCRY_CIPHER_MODE_CBC, 0)) {
          SAFE_FREE(cipher->key);
          return -1;
        }
        break;
      case 192:
        if (gcry_cipher_open(&cipher->key[0], GCRY_CIPHER_AES192,
              GCRY_CIPHER_MODE_CBC, 0)) {
          SAFE_FREE(cipher->key);
          return -1;
        }
        break;
      case 256:
        if (gcry_cipher_open(&cipher->key[0], GCRY_CIPHER_AES256,
              GCRY_CIPHER_MODE_CBC, 0)) {
          SAFE_FREE(cipher->key);
          return -1;
        }
        break;
    }
    if (gcry_cipher_setkey(cipher->key[0], key, cipher->keysize / 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setiv(cipher->key[0], IV, 16)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
  }

  return 0;
}

static void aes_encrypt(struct crypto_struct *cipher, void *in, void *out,
    unsigned long len) {
  gcry_cipher_encrypt(cipher->key[0], out, len, in, len);
}

static void aes_decrypt(struct crypto_struct *cipher, void *in, void *out,
    unsigned long len) {
  gcry_cipher_decrypt(cipher->key[0], out, len, in, len);
}

static int des3_set_key(struct crypto_struct *cipher, void *key, void *IV) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }
    if (gcry_cipher_open(&cipher->key[0], GCRY_CIPHER_3DES,
          GCRY_CIPHER_MODE_CBC, 0)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setkey(cipher->key[0], key, 24)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setiv(cipher->key[0], IV, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
  }

  return 0;
}

static void des3_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len) {
  gcry_cipher_encrypt(cipher->key[0], out, len, in, len);
}

static void des3_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len) {
  gcry_cipher_decrypt(cipher->key[0], out, len, in, len);
}

static int des3_1_set_key(struct crypto_struct *cipher, void *key, void *IV) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }
    if (gcry_cipher_open(&cipher->key[0], GCRY_CIPHER_DES,
          GCRY_CIPHER_MODE_CBC, 0)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setkey(cipher->key[0], key, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setiv(cipher->key[0], IV, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }

    if (gcry_cipher_open(&cipher->key[1], GCRY_CIPHER_DES,
          GCRY_CIPHER_MODE_CBC, 0)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setkey(cipher->key[1], key + 8, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setiv(cipher->key[1], IV + 8, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }

    if (gcry_cipher_open(&cipher->key[2], GCRY_CIPHER_DES,
          GCRY_CIPHER_MODE_CBC, 0)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setkey(cipher->key[2], key + 16, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
    if (gcry_cipher_setiv(cipher->key[2], IV + 16, 8)) {
      SAFE_FREE(cipher->key);
      return -1;
    }
  }

  return 0;
}

static void des3_1_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len) {
  gcry_cipher_encrypt(cipher->key[0], out, len, in, len);
  gcry_cipher_decrypt(cipher->key[1], in, len, out, len);
  gcry_cipher_encrypt(cipher->key[2], out, len, in, len);
}

static void des3_1_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len) {
  gcry_cipher_decrypt(cipher->key[2], out, len, in, len);
  gcry_cipher_encrypt(cipher->key[1], in, len, out, len);
  gcry_cipher_decrypt(cipher->key[0], out, len, in, len);
}

/* the table of supported ciphers */
static struct crypto_struct ssh_ciphertab[] = {
  {
    .name            = "blowfish-cbc",
    .blocksize       = 8,
    .keylen          = sizeof(gcry_cipher_hd_t),
    .key             = NULL,
    .keysize         = 128,
    .set_encrypt_key = blowfish_set_key,
    .set_decrypt_key = blowfish_set_key,
    .cbc_encrypt     = blowfish_encrypt,
    .cbc_decrypt     = blowfish_decrypt
  },
  {
    .name            = "aes128-cbc",
    .blocksize       = 16,
    .keylen          = sizeof(gcry_cipher_hd_t),
    .key             = NULL,
    .keysize         = 128,
    .set_encrypt_key = aes_set_key,
    .set_decrypt_key = aes_set_key,
    .cbc_encrypt     = aes_encrypt,
    .cbc_decrypt     = aes_decrypt
  },
  {
    .name            = "aes192-cbc",
    .blocksize       = 16,
    .keylen          = sizeof(gcry_cipher_hd_t),
    .key             = NULL,
    .keysize         = 192,
    .set_encrypt_key = aes_set_key,
    .set_decrypt_key = aes_set_key,
    .cbc_encrypt     = aes_encrypt,
    .cbc_decrypt     = aes_decrypt
  },
  {
    .name            = "aes256-cbc",
    .blocksize       = 16,
    .keylen          = sizeof(gcry_cipher_hd_t),
    .key             = NULL,
    .keysize         = 256,
    .set_encrypt_key = aes_set_key,
    .set_decrypt_key = aes_set_key,
    .cbc_encrypt     = aes_encrypt,
    .cbc_decrypt     = aes_decrypt
  },
  {
    .name            = "3des-cbc",
    .blocksize       = 8,
    .keylen          = sizeof(gcry_cipher_hd_t),
    .key             = NULL,
    .keysize         = 192,
    .set_encrypt_key = des3_set_key,
    .set_decrypt_key = des3_set_key,
    .cbc_encrypt     = des3_encrypt,
    .cbc_decrypt     = des3_decrypt
  },
  {
    .name            = "3des-cbc-ssh1",
    .blocksize       = 8,
    .keylen          = sizeof(gcry_cipher_hd_t) * 3,
    .key             = NULL,
    .keysize         = 192,
    .set_encrypt_key = des3_1_set_key,
    .set_decrypt_key = des3_1_set_key,
    .cbc_encrypt     = des3_1_encrypt,
    .cbc_decrypt     = des3_1_decrypt
  },
  {
    .name            = NULL,
    .blocksize       = 0,
    .keylen          = 0,
    .key             = NULL,
    .keysize         = 0,
    .set_encrypt_key = NULL,
    .set_decrypt_key = NULL,
    .cbc_encrypt     = NULL,
    .cbc_decrypt     = NULL
  }
};

#elif defined HAVE_LIBCRYPTO

#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/hmac.h>
#include <openssl/opensslv.h>
#ifdef HAVE_OPENSSL_AES_H
#define HAS_AES
#include <openssl/aes.h>
#endif
#ifdef HAVE_OPENSSL_BLOWFISH_H
#define HAS_BLOWFISH
#include <openssl/blowfish.h>
#endif
#ifdef HAVE_OPENSSL_DES_H
#define HAS_DES
#include <openssl/des.h>
#endif

#if (OPENSSL_VERSION_NUMBER<0x009070000)
#define OLD_CRYPTO
#endif

#include "libssh/crypto.h"

static int alloc_key(struct crypto_struct *cipher) {
    cipher->key = malloc(cipher->keylen);
    if (cipher->key == NULL) {
      return -1;
    }

    return 0;
}

SHACTX sha1_init(void) {
  SHACTX c = malloc(sizeof(*c));
  if (c == NULL) {
    return NULL;
  }
  SHA1_Init(c);

  return c;
}

void sha1_update(SHACTX c, const void *data, unsigned long len) {
  SHA1_Update(c,data,len);
}

void sha1_final(unsigned char *md, SHACTX c) {
  SHA1_Final(md, c);
  SAFE_FREE(c);
}

void sha1(unsigned char *digest, int len, unsigned char *hash) {
  SHA1(digest, len, hash);
}

MD5CTX md5_init(void) {
  MD5CTX c = malloc(sizeof(*c));
  if (c == NULL) {
    return NULL;
  }

  MD5_Init(c);

  return c;
}

void md5_update(MD5CTX c, const void *data, unsigned long len) {
  MD5_Update(c, data, len);
}

void md5_final(unsigned char *md, MD5CTX c) {
  MD5_Final(md,c);
  SAFE_FREE(c);
}

HMACCTX hmac_init(const void *key, int len, int type) {
  HMACCTX ctx = NULL;

  ctx = malloc(sizeof(*ctx));
  if (ctx == NULL) {
    return NULL;
  }

#ifndef OLD_CRYPTO
  HMAC_CTX_init(ctx); // openssl 0.9.7 requires it.
#endif

  switch(type) {
    case HMAC_SHA1:
      HMAC_Init(ctx, key, len, EVP_sha1());
      break;
    case HMAC_MD5:
      HMAC_Init(ctx, key, len, EVP_md5());
      break;
    default:
      SAFE_FREE(ctx);
      ctx = NULL;
  }

  return ctx;
}

void hmac_update(HMACCTX ctx, const void *data, unsigned long len) {
  HMAC_Update(ctx, data, len);
}

void hmac_final(HMACCTX ctx, unsigned char *hashmacbuf, unsigned int *len) {
  HMAC_Final(ctx,hashmacbuf,len);

#ifndef OLD_CRYPTO
  HMAC_CTX_cleanup(ctx);
#else
  HMAC_cleanup(ctx);
#endif

  SAFE_FREE(ctx);
}

#ifdef HAS_BLOWFISH
/* the wrapper functions for blowfish */
static int blowfish_set_key(struct crypto_struct *cipher, void *key){
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }
    BF_set_key(cipher->key, 16, key);
  }

  return 0;
}

static void blowfish_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  BF_cbc_encrypt(in, out, len, cipher->key, IV, BF_ENCRYPT);
}

static void blowfish_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  BF_cbc_encrypt(in, out, len, cipher->key, IV, BF_DECRYPT);
}
#endif /* HAS_BLOWFISH */

#ifdef HAS_AES
static int aes_set_encrypt_key(struct crypto_struct *cipher, void *key) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }
    if (AES_set_encrypt_key(key,cipher->keysize,cipher->key) < 0) {
      SAFE_FREE(cipher->key);
      return -1;
    }
  }

  return 0;
}
static int aes_set_decrypt_key(struct crypto_struct *cipher, void *key) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }
    if (AES_set_decrypt_key(key,cipher->keysize,cipher->key) < 0) {
      SAFE_FREE(cipher->key);
      return -1;
    }
  }

  return 0;
}

static void aes_encrypt(struct crypto_struct *cipher, void *in, void *out,
    unsigned long len, void *IV) {
  AES_cbc_encrypt(in, out, len, cipher->key, IV, AES_ENCRYPT);
}

static void aes_decrypt(struct crypto_struct *cipher, void *in, void *out,
    unsigned long len, void *IV) {
  AES_cbc_encrypt(in, out, len, cipher->key, IV, AES_DECRYPT);
}
#endif /* HAS_AES */

#ifdef HAS_DES
static int des3_set_key(struct crypto_struct *cipher, void *key) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }

    DES_set_odd_parity(key);
    DES_set_odd_parity(key + 8);
    DES_set_odd_parity(key + 16);
    DES_set_key_unchecked(key, cipher->key);
    DES_set_key_unchecked(key + 8, cipher->key + sizeof(DES_key_schedule));
    DES_set_key_unchecked(key + 16, cipher->key + 2 * sizeof(DES_key_schedule));
  }

  return 0;
}

static void des3_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  DES_ede3_cbc_encrypt(in, out, len, cipher->key,
      cipher->key + sizeof(DES_key_schedule),
      cipher->key + 2 * sizeof(DES_key_schedule),
      IV, 1);
}

static void des3_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  DES_ede3_cbc_encrypt(in, out, len, cipher->key,
      cipher->key + sizeof(DES_key_schedule),
      cipher->key + 2 * sizeof(DES_key_schedule),
      IV, 0);
}

static void des3_1_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
#ifdef DEBUG_CRYPTO
  ssh_print_hexa("Encrypt IV before", IV, 24);
#endif
  DES_ncbc_encrypt(in, out, len, cipher->key, IV, 1);
  DES_ncbc_encrypt(out, in, len, cipher->key + sizeof(DES_key_schedule),
      IV + 8, 0);
  DES_ncbc_encrypt(in, out, len, cipher->key + 2 * sizeof(DES_key_schedule),
      IV + 16, 1);
#ifdef DEBUG_CRYPTO
  ssh_print_hexa("Encrypt IV after", IV, 24);
#endif
}

static void des3_1_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
#ifdef DEBUG_CRYPTO
  ssh_print_hexa("Decrypt IV before", IV, 24);
#endif

  DES_ncbc_encrypt(in, out, len, cipher->key + 2 * sizeof(DES_key_schedule),
      IV, 0);
  DES_ncbc_encrypt(out, in, len, cipher->key + sizeof(DES_key_schedule),
      IV + 8, 1);
  DES_ncbc_encrypt(in, out, len, cipher->key, IV + 16, 0);

#ifdef DEBUG_CRYPTO
  ssh_print_hexa("Decrypt IV after", IV, 24);
#endif
}

#endif /* HAS_DES */

/* the table of supported ciphers */
static struct crypto_struct ssh_ciphertab[] = {
#ifdef HAS_BLOWFISH
  {
    .name            = "blowfish-cbc",
    .blocksize       = 8,
    .keylen          = sizeof (BF_KEY),
    .key             = NULL,
    .keysize         = 128,
    .set_encrypt_key = blowfish_set_key,
    .set_decrypt_key = blowfish_set_key,
    .cbc_encrypt     = blowfish_encrypt,
    .cbc_decrypt     = blowfish_decrypt
  },
#endif /* HAS_BLOWFISH */
#ifdef HAS_AES
  {
    .name            = "aes128-cbc",
    .blocksize       = 16,
    .keylen          = sizeof(AES_KEY),
    .key             = NULL,
    .keysize         = 128,
    .set_encrypt_key = aes_set_encrypt_key,
    .set_decrypt_key = aes_set_decrypt_key,
    .cbc_encrypt     = aes_encrypt,
    .cbc_decrypt     = aes_decrypt
  },
  {
    .name            = "aes192-cbc",
    .blocksize       = 16,
    .keylen          = sizeof(AES_KEY),
    .key             = NULL,
    .keysize         = 192,
    .set_encrypt_key = aes_set_encrypt_key,
    .set_decrypt_key = aes_set_decrypt_key,
    .cbc_encrypt     = aes_encrypt,
    .cbc_decrypt     = aes_decrypt
  },
  {
    .name            = "aes256-cbc",
    .blocksize       = 16,
    .keylen          = sizeof(AES_KEY),
    .key             = NULL,
    .keysize         = 256,
    .set_encrypt_key = aes_set_encrypt_key,
    .set_decrypt_key = aes_set_decrypt_key,
    .cbc_encrypt     = aes_encrypt,
    .cbc_decrypt     = aes_decrypt
  },
#endif /* HAS_AES */
#ifdef HAS_DES
  {
    .name            = "3des-cbc",
    .blocksize       = 8,
    .keylen          = sizeof(DES_key_schedule) * 3,
    .key             = NULL,
    .keysize         = 192,
    .set_encrypt_key = des3_set_key,
    .set_decrypt_key = des3_set_key,
    .cbc_encrypt     = des3_encrypt,
    .cbc_decrypt     = des3_decrypt
  },
  {
    .name            = "3des-cbc-ssh1",
    .blocksize       = 8,
    .keylen          = sizeof(DES_key_schedule) * 3,
    .key             = NULL,
    .keysize         = 192,
    .set_encrypt_key = des3_set_key,
    .set_decrypt_key = des3_set_key,
    .cbc_encrypt     = des3_1_encrypt,
    .cbc_decrypt     = des3_1_decrypt
  },
#endif /* HAS_DES */
  {
    .name            = NULL,
    .blocksize       = 0,
    .keylen          = 0,
    .key             = NULL,
    .keysize         = 0,
    .set_encrypt_key = NULL,
    .set_decrypt_key = NULL,
    .cbc_encrypt     = NULL,
    .cbc_decrypt     = NULL
  }
};
#endif /* OPENSSL_CRYPTO */

/* it allocates a new cipher structure based on its offset into the global table */
static struct crypto_struct *cipher_new(int offset) {
  struct crypto_struct *cipher = NULL;

  cipher = malloc(sizeof(struct crypto_struct));
  if (cipher == NULL) {
    return NULL;
  }

  /* note the memcpy will copy the pointers : so, you shouldn't free them */
  memcpy(cipher, &ssh_ciphertab[offset], sizeof(*cipher));

  return cipher;
}

static void cipher_free(struct crypto_struct *cipher) {
#ifdef HAVE_LIBGCRYPT
  unsigned int i;
#endif

  if (cipher == NULL) {
    return;
  }

  if(cipher->key) {
#ifdef HAVE_LIBGCRYPT
    for (i = 0; i < (cipher->keylen / sizeof(gcry_cipher_hd_t)); i++) {
      gcry_cipher_close(cipher->key[i]);
    }
#elif defined HAVE_LIBCRYPTO
    /* destroy the key */
    memset(cipher->key, 0, cipher->keylen);
#endif
    SAFE_FREE(cipher->key);
  }
  SAFE_FREE(cipher);
}

CRYPTO *crypto_new(void) {
  CRYPTO *crypto;

  crypto = malloc(sizeof(CRYPTO));
  if (crypto == NULL) {
    return NULL;
  }

  memset(crypto, 0, sizeof(CRYPTO));

  return crypto;
}

void crypto_free(CRYPTO *crypto){
  if (crypto == NULL) {
    return;
  }

  SAFE_FREE(crypto->server_pubkey);

  cipher_free(crypto->in_cipher);
  cipher_free(crypto->out_cipher);

  bignum_free(crypto->e);
  bignum_free(crypto->f);
  bignum_free(crypto->x);
  bignum_free(crypto->y);
  bignum_free(crypto->k);
  /* lot of other things */
  /* i'm lost in my own code. good work */
  memset(crypto,0,sizeof(*crypto));

  SAFE_FREE(crypto);
}

static int crypt_set_algorithms2(SSH_SESSION *session){
  const char *wanted;
  int i = 0;

  /* we must scan the kex entries to find crypto algorithms and set their appropriate structure */
  /* out */
  wanted = session->client_kex.methods[SSH_CRYPT_C_S];
  while (ssh_ciphertab[i].name && strcmp(wanted, ssh_ciphertab[i].name)) {
    i++;
  }

  if (ssh_ciphertab[i].name == NULL) {
    ssh_set_error(session, SSH_FATAL,
        "Crypt_set_algorithms2: no crypto algorithm function found for %s",
        wanted);
    return SSH_ERROR;
  }
  ssh_log(session, SSH_LOG_PACKET, "Set output algorithm to %s", wanted);

  session->next_crypto->out_cipher = cipher_new(i);
  if (session->next_crypto->out_cipher == NULL) {
    ssh_set_error(session, SSH_FATAL, "No space left");
    return SSH_ERROR;
  }
  i = 0;

  /* in */
  wanted = session->client_kex.methods[SSH_CRYPT_S_C];
  while (ssh_ciphertab[i].name && strcmp(wanted, ssh_ciphertab[i].name)) {
    i++;
  }

  if (ssh_ciphertab[i].name == NULL) {
    ssh_set_error(session, SSH_FATAL,
        "Crypt_set_algorithms: no crypto algorithm function found for %s",
        wanted);
    return SSH_ERROR;
  }
  ssh_log(session, SSH_LOG_PACKET, "Set input algorithm to %s", wanted);

  session->next_crypto->in_cipher = cipher_new(i);
  if (session->next_crypto->in_cipher == NULL) {
    ssh_set_error(session, SSH_FATAL, "Not enough space");
    return SSH_ERROR;
  }

  /* compression */
  if (strstr(session->client_kex.methods[SSH_COMP_C_S], "zlib")) {
    session->next_crypto->do_compress_out = 1;
  }
  if (strstr(session->client_kex.methods[SSH_COMP_S_C], "zlib")) {
    session->next_crypto->do_compress_in = 1;
  }

  return SSH_OK;
}

static int crypt_set_algorithms1(SSH_SESSION *session) {
  int i = 0;

  /* right now, we force 3des-cbc to be taken */
  while (ssh_ciphertab[i].name && strcmp(ssh_ciphertab[i].name,
        "3des-cbc-ssh1")) {
    i++;
  }

  if (ssh_ciphertab[i].name == NULL) {
    ssh_set_error(session, SSH_FATAL, "cipher 3des-cbc-ssh1 not found!");
    return -1;
  }

  session->next_crypto->out_cipher = cipher_new(i);
  if (session->next_crypto->out_cipher == NULL) {
    ssh_set_error(session, SSH_FATAL, "No space left");
    return SSH_ERROR;
  }

  session->next_crypto->in_cipher = cipher_new(i);
  if (session->next_crypto->in_cipher == NULL) {
    ssh_set_error(session, SSH_FATAL, "No space left");
    return SSH_ERROR;
  }

  return SSH_OK;
}

int crypt_set_algorithms(SSH_SESSION *session) {
  return (session->version == 1) ? crypt_set_algorithms1(session) :
    crypt_set_algorithms2(session);
}

// TODO Obviously too much cut and paste here
int crypt_set_algorithms_server(SSH_SESSION *session){
    char *server = NULL;
    char *client = NULL;
    char *match = NULL;
    int i = 0;

    /* we must scan the kex entries to find crypto algorithms and set their appropriate structure */
    enter_function();
    /* out */
    server = session->server_kex.methods[SSH_CRYPT_S_C];
    client = session->client_kex.methods[SSH_CRYPT_S_C];
    match = ssh_find_matching(client,server);

    if(!match){
        ssh_set_error(session,SSH_FATAL,"Crypt_set_algorithms_server : no matching algorithm function found for %s",server);
        free(match);
        leave_function();
        return SSH_ERROR;
    }
    while(ssh_ciphertab[i].name && strcmp(match,ssh_ciphertab[i].name))
        i++;
    if(!ssh_ciphertab[i].name){
        ssh_set_error(session,SSH_FATAL,"Crypt_set_algorithms_server : no crypto algorithm function found for %s",server);
        free(match);
        leave_function();
        return SSH_ERROR;
    }
    ssh_log(session,SSH_LOG_PACKET,"Set output algorithm %s",match);
    SAFE_FREE(match);

    session->next_crypto->out_cipher = cipher_new(i);
    if (session->next_crypto->out_cipher == NULL) {
      ssh_set_error(session, SSH_FATAL, "No space left");
      leave_function();
      return SSH_ERROR;
    }
    i=0;
    /* in */
    client=session->client_kex.methods[SSH_CRYPT_C_S];
    server=session->server_kex.methods[SSH_CRYPT_S_C];
    match=ssh_find_matching(client,server);
    if(!match){
        ssh_set_error(session,SSH_FATAL,"Crypt_set_algorithms_server : no matching algorithm function found for %s",server);
        free(match);
        leave_function();
        return SSH_ERROR;
    }
    while(ssh_ciphertab[i].name && strcmp(match,ssh_ciphertab[i].name))
        i++;
    if(!ssh_ciphertab[i].name){
        ssh_set_error(session,SSH_FATAL,"Crypt_set_algorithms_server : no crypto algorithm function found for %s",server);
        free(match);
        leave_function();
        return SSH_ERROR;
    }
    ssh_log(session,SSH_LOG_PACKET,"Set input algorithm %s",match);
    SAFE_FREE(match);

    session->next_crypto->in_cipher = cipher_new(i);
    if (session->next_crypto->in_cipher == NULL) {
      ssh_set_error(session, SSH_FATAL, "No space left");
      leave_function();
      return SSH_ERROR;
    }

    /* compression */
    client=session->client_kex.methods[SSH_CRYPT_C_S];
    server=session->server_kex.methods[SSH_CRYPT_C_S];
    match=ssh_find_matching(client,server);
    if(match && !strcmp(match,"zlib")){
        ssh_log(session,SSH_LOG_PACKET,"enabling C->S compression");
        session->next_crypto->do_compress_in=1;
    }
    free(match);
    
    client=session->client_kex.methods[SSH_CRYPT_S_C];
    server=session->server_kex.methods[SSH_CRYPT_S_C];
    match=ssh_find_matching(client,server);
    if(match && !strcmp(match,"zlib")){
        ssh_log(session,SSH_LOG_PACKET,"enabling S->C compression\n");
        session->next_crypto->do_compress_out=1;
    }
    free(match);
    
    server=session->server_kex.methods[SSH_HOSTKEYS];
    client=session->client_kex.methods[SSH_HOSTKEYS];
    match=ssh_find_matching(client,server);
    if(!strcmp(match,"ssh-dss"))
        session->hostkeys=TYPE_DSS;
    else if(!strcmp(match,"ssh-rsa"))
        session->hostkeys=TYPE_RSA;
    else {
        ssh_set_error(session,SSH_FATAL,"cannot know what %s is into %s",match,server);
        free(match);
        leave_function();
        return SSH_ERROR;
    }
    free(match);
    leave_function();
    return SSH_OK;
}
