#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

void emit(const char* name, const char* text, unsigned char key)
{
    size_t len = strlen(text);
    printf("unsigned char %s[] = { ", name);
    for (size_t i = 0; i < len; i++)
        printf("0x%02X, ", (unsigned char)text[i] ^ key);
    printf("0x%02X };  // \"%s\"\n", key, text);
}

int main()
{
    unsigned char key = 0x5A;   

    emit("enc_err_openpwd", "Error opening password file.", key);
    emit("enc_err_readpwd", "Error reading password file.", key);
    emit("enc_pwd_read", "Password read from file: %s\n", key);
    emit("enc_dbg1", "Debugger detected. Exiting.", key);
    emit("enc_fake", "Security violation code 0x1F.", key);
    emit("enc_access_viol", "Access violation.", key);
    emit("enc_err_output", "Error opening output file.", key);
    emit("enc_correct", "Password is correct.", key);
    emit("enc_incorrect", "Password is incorrect.", key);
    emit("enc_decoy", "Access denied (decoy).", key);
    emit("enc_integrity", "Integrity check failed. Code tampered.", key);
    emit("enc_finished", "Program finished.", key);
    emit("enc_err_file", "Error opening file!", key);
    return 0;
}