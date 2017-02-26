/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include "crypto.h"
#include "utils.h"

//Our magic number that's written into the
//encrypted file. Used to determine if the file
//is encrypted.
static const int MAGIC_HEADER = 0x33497546;

//Function generates random data from /dev/urandom
//Parameter size is how much random data caller
//wants to generate. Caller must free the return value.
//Returns the data or NULL on failure.
static char *generate_random_data(int size)
{
    char *data = NULL;
    FILE *frnd = NULL;

    data = tmalloc(size * sizeof(char));

    frnd = fopen("/dev/urandom", "r");

    if(!frnd)
    {
        fprintf(stderr, "Cannot open /dev/urandom\n");
        free(data);
        return NULL;
    }

    fread(data, size, 1, frnd);
    fclose(frnd);

    return data;
}

//Generate key from passphrase. If oldsalt is NULL, new salt is created.
//ok is set to true on success, false on failure
static Key_t generate_key(const char *passphrase, char *old_salt,
                          bool *ok)
{
    char *salt = NULL;
    int iterations = 25000;
    Key_t key = { {0} };
    int success;
    char *resultbytes[KEY_SIZE];

    if(old_salt == NULL)
        salt = generate_random_data(SALT_SIZE);
    else
    {
        salt = tmalloc(SALT_SIZE);
        memmove(salt, old_salt, SALT_SIZE);
    }

    if(!salt)
    {
        *ok = false;
        return key;
    }

    success = PKCS5_PBKDF2_HMAC(passphrase, strlen(passphrase), (unsigned char*)salt,
                                SALT_SIZE, iterations, EVP_sha256(),
                                KEY_SIZE, (unsigned char*)resultbytes);

    if(success == 0)
    {
        free(salt);
        *ok = false;
        return key;
    }

    memmove(key.data, resultbytes, KEY_SIZE);
    memmove(key.salt, salt, SALT_SIZE);

    free(salt);
    *ok = true;

    return key;
}

//Function appends ext to the orig string.
//Returns new string on success, NULL on failure.
//Caller must free the return value.
static char *get_output_filename(const char *orig, const char *ext)
{
    char *path = NULL;
    size_t len;

    len = strlen(orig) + strlen(ext) + 1;
    path = tmalloc(len * sizeof(char));

    strcpy(path, orig);
    strcat(path, ext);

    return path;
}

static bool encrypt_decrypt(unsigned char *data_in, int data_in_len,
                            FILE *out, unsigned char *key,
                            unsigned char *iv, int is_encrypt)
{
    EVP_CIPHER_CTX *ctx;
    unsigned char *out_buffer = NULL;
    int output_len = 0;
    int output_len_final = 0;
    int cipher_block_size;

    ctx = EVP_CIPHER_CTX_new();

    if(EVP_CipherInit(ctx, EVP_aes_256_ctr(), key, iv, is_encrypt) != 1)
    {
        fprintf(stderr, "Unable to initialize AES.\n");
        return false;
    }

    cipher_block_size = EVP_CIPHER_CTX_block_size(ctx);
    out_buffer = tmalloc(data_in_len +(cipher_block_size));

    if(EVP_CipherUpdate(ctx, out_buffer, &output_len, data_in, data_in_len) != 1)
    {
        fprintf(stderr, "Unable to process data.\n");
        free(out_buffer);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if(EVP_CipherFinal(ctx, out_buffer + output_len, &output_len_final) != 1)
    {
        fprintf(stderr, "Unable to finalize.\n");
        free(out_buffer);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    fwrite(out_buffer, sizeof(unsigned char), output_len + output_len_final, out);

    free(out_buffer);

    EVP_CIPHER_CTX_free(ctx);

    return true;
}

static unsigned char *hmac_data(const void *key, int key_len,
                               unsigned char *data, int data_len,
                               unsigned char *result, int *res_len)
{
    return HMAC(EVP_sha512(), key, key_len, data, data_len, result,
                (unsigned int *)res_len);
}

//Calculates hmac from the content of fp and writes the hash
//into end of the file. Caller must close fp.
static bool calculate_and_write_hmac(FILE *fp, const void *key)
{
    unsigned char *hmac_sha512 = NULL;
    char *cipher_buffer = NULL;
    int len = 0;
    int cipherlen;

    fseek(fp, 0, SEEK_END);
    cipherlen = ftell(fp);
    //Cursor back to beginning so we can read the data for hmac
    fseek(fp, 0, SEEK_SET);

    cipher_buffer = tmalloc(cipherlen * sizeof(char));
    fread(cipher_buffer, sizeof(char), cipherlen, fp);

    hmac_sha512 = tmalloc(HMAC_SHA512_SIZE);

    hmac_data(key, KEY_SIZE, (unsigned char *)cipher_buffer,cipherlen,
              (unsigned char *)hmac_sha512, &len);

	fseek(fp, 0, SEEK_END);
    fwrite(hmac_sha512, 1, HMAC_SHA512_SIZE, fp);

    free(cipher_buffer);
    free(hmac_sha512);

    return true;
}

//Function assumes that fp cursor is in the right place
//to read the stored hmac from the file.
static bool read_and_verify_hmac(const char *path, char *hmac, const void *key)
{
    char *new_hmac = tmalloc(HMAC_SHA512_SIZE);
    char *buffer;
    int len;
    int offset;
    int result;
    FILE *fp = NULL;
    bool retval = false;

    fp = fopen(path, "r");

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);

    //calculate the actual hmacced data size
    offset = len - HMAC_SHA512_SIZE;
    fseek(fp, 0, SEEK_SET);

    buffer = tmalloc(offset * sizeof(char));

    //read whole file until hmac
    fread(buffer, sizeof(char), offset, fp);

    hmac_data(key, KEY_SIZE, (unsigned char*)buffer, offset,
             (unsigned char*)new_hmac, &result);

    if(CRYPTO_memcmp(hmac, new_hmac, HMAC_SHA512_SIZE) == 0)
        retval = true;

    free(new_hmac);
    free(buffer);
    fclose(fp);

    return retval;
}

//This function really just checks is the file
//written using Titan.
bool is_file_encrypted(const char *path)
{
    FILE *fp = NULL;
    int data;

    fp = fopen(path, "r");

    if(!fp)
    {
        fprintf(stderr, "Failed to open file.\n");
        return false;
    }

    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    int offset = sizeof(int) + IV_SIZE + SALT_SIZE + HMAC_SHA512_SIZE;
    rewind(fp);

    fseek(fp, len - offset, SEEK_CUR);

    //Read our magic header
    fread((void*)&data, sizeof(MAGIC_HEADER), 1, fp);
    fclose(fp);

    if(data != MAGIC_HEADER)
        return false;

    return true;
}

bool encrypt_file(const char *passphrase, const char *path)
{
    bool ok;
    char *iv = NULL;
    FILE *plain = NULL;
    FILE *cipher_fp = NULL;
    char *output_filename = NULL;
    char *plain_data = NULL;

    if(is_file_encrypted(path))
    {
        fprintf(stderr, "File is already encrypted.\n");
        return false;
    }

    Key_t key = generate_key(passphrase, NULL, &ok);

    if(!ok)
    {
        fprintf(stderr, "Key derivation failed.\n");
        return false;
    }

    iv = generate_random_data(IV_SIZE);

    if(!iv)
    {
        fprintf(stderr, "Initialization vector generation failed.\n");
        return false;
    }

    plain = fopen(path, "r");

    if(!plain)
    {
        fprintf(stderr, "Unable to open %s\n", path);
        free(iv);
        return false;
    }

    fseek(plain, 0, SEEK_END);
    int plain_len = ftell(plain);
    fseek(plain, 0, SEEK_SET);

    plain_data = tmalloc(plain_len * sizeof(char));

    fread(plain_data, sizeof(char), plain_len, plain);
    fclose(plain);

    output_filename = get_output_filename(path, ".titan");

    if(!output_filename)
    {
        fprintf(stderr, "Unable to create output filename.\n");
        free(iv);
        free(plain_data);
        return false;
    }

    cipher_fp = fopen(output_filename, "w");

    if(!cipher_fp)
    {
        fprintf(stderr, "Unable to open %s for writing.\n", output_filename);
        free(iv);
        free(output_filename);
        free(plain_data);
        return false;
    }

    //perform the actual encryption
    encrypt_decrypt((unsigned char*)plain_data, plain_len, cipher_fp,
                    (unsigned char *)key.data, (unsigned char *)iv,
                    TITAN_MODE_ENCRYPT);

    //write iv etc. into the end of the file
    fwrite((void*)&MAGIC_HEADER, sizeof(MAGIC_HEADER), 1, cipher_fp);
    fwrite(iv, 1, IV_SIZE, cipher_fp);
    fwrite(key.salt, 1, SALT_SIZE, cipher_fp);

    //Close the file pointer, to sync the data, before reading it again
    //for the hmac calculation
    fclose(cipher_fp);

    //Open the file again for reading and writing
    cipher_fp = fopen(output_filename, "r+");

    if(!calculate_and_write_hmac(cipher_fp, key.data))
    {
        free(iv);
        free(output_filename);
        free(plain_data);
        fclose(cipher_fp);

        return false;
    }

    //Finally remove the plain file
    if(remove(path) != 0)
        fprintf(stderr, "WARNING: Error deleting plain file %s.", path);

    //And rename our ciphered file back to the original name
    rename(output_filename, path);
    free(output_filename);
    free(plain_data);
    free(iv);
    fclose(cipher_fp);

    return true;
}

bool decrypt_file(const char *passphrase, const char *path)
{
    bool ok;
    char *iv = NULL;
    char *salt = NULL;
    FILE *plain = NULL;
    FILE *cipher = NULL;
    char *output_filename = NULL;
    char *cipher_data = NULL;
    char *hmac;

    if(!is_file_encrypted(path))
    {
        fprintf(stderr, "File is already decrypted or malformed.\n");
        return false;
    }

    iv = tmalloc(IV_SIZE);
    salt = tmalloc(SALT_SIZE);

    cipher = fopen(path, "r");

    if(!cipher)
    {
        fprintf(stderr, "Unable to open %s for reading.\n", path);
        free(iv);
        free(salt);
        return false;
    }

    fseek(cipher, 0, SEEK_END);
    int len = ftell(cipher);
    int offset = len - (sizeof(int) + IV_SIZE + SALT_SIZE + HMAC_SHA512_SIZE);

    rewind(cipher);

    hmac = tmalloc(HMAC_SHA512_SIZE);

    fseek(cipher, offset, SEEK_CUR);
    //Skip the magic header
    fseek(cipher, sizeof(int), SEEK_CUR);
    //iv, salt
    fread(iv, IV_SIZE, 1, cipher);
    fread(salt, SALT_SIZE, 1, cipher);
    fread(hmac, HMAC_SHA512_SIZE, 1, cipher);

    Key_t key = generate_key(passphrase, salt, &ok);

    if(!ok)
    {
        fprintf(stderr, "Key derivation failed.\n");
        free(iv);
        free(salt);
        free(hmac);
        fclose(cipher);
        return false;
    }

    fclose(cipher);

    if(!read_and_verify_hmac(path, hmac, key.data))
    {
        fprintf(stderr, "Invalid password or tampered data. Aborted.\n");
        free(iv);
        free(salt);
        free(hmac);
        
        return false;
    }

    cipher = fopen(path, "r");
    cipher_data = tmalloc(offset * sizeof(char));

    //read all data, skip header, salt, iv
    fread(cipher_data, sizeof(char), offset, cipher);
    fclose(cipher);

    output_filename = get_output_filename(path, ".plain");

    if(!output_filename)
    {
        fprintf(stderr, "Unable to create output filename.\n");
        free(iv);
        free(salt);
        free(cipher_data);
        free(hmac);

        return false;
    }

    plain = fopen(output_filename, "w");

    if(!plain)
    {
        fprintf(stderr, "Unable to open %s for writing.\n", output_filename);
        free(iv);
        free(salt);
        free(output_filename);
        free(cipher_data);
        free(hmac);

        return false;
    }

    encrypt_decrypt((unsigned char*)cipher_data, offset, plain,
                    (unsigned char *)key.data, (unsigned char *)iv,
                    TITAN_MODE_DECRYPT);

    //Finally remove the cipher file
    if(remove(path) != 0)
    {
        fprintf(stderr, "WARNING: Error deleting file %s.", path);
    }

    //And rename our ciphered file back to the original name
    rename(output_filename, path);
    free(output_filename);

    free(iv);
    free(salt);
    fclose(plain);
    free(cipher_data);
    free(hmac);

    return true;
}
