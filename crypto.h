/*
 * Copyright (C) 2018 Niko Rosvall <niko@byteptr.com>
 */

#ifndef __CRYPTO_H
#define __CRYPTO_H

#define KEY_SIZE (32)  //256 bits
#define IV_SIZE (16)   //128 bits
#define SALT_SIZE (64) //512 bits
#define HMAC_SHA512_SIZE (64)

#define TITAN_MODE_DECRYPT (0)
#define TITAN_MODE_ENCRYPT (1)

typedef struct Key
{
    char data[32];
    char salt[64];

} Key_t;

bool encrypt_file(const char *passphrase, const char *path);
bool decrypt_file(const char *passphrase, const char *path);
bool is_file_encrypted(const char *path);

#endif
