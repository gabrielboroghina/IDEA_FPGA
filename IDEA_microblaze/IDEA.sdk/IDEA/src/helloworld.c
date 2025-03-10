#include <stdio.h>
#include <stdint.h>
#include "platform.h"
#include "xil_printf.h"

#define MOD 65537

char key_txt[128] = "00000000011001000000000011001000000000010010110000000001100100000000000111110100"
                    "000000100101100000000010101111000000001100100000";

uint16_t msg[10], EK[52], DK[52];

uint16_t key_bit(int i) {
    i %= 128;
    return key_txt[i] == '1' ? (uint16_t) 1 : (uint16_t) 0;
}

void generate_encrypt_keys() {
    int nr = 0, start = 0;

    for (int i = 1; i <= 7; i++) {
        int nr_subkeys = i < 7 ? 8 : 4;

        for (int j = 0; j < nr_subkeys; j++) {
            for (int k = 0; k < 16; k++)
                EK[nr] |= key_bit(start + j * 16 + k) << (15 - k);
            nr++;
        }

        // shift the key
        start = (start + 25) % 128;
    }

    for (int i = 0; i < 52; i++) {
        xil_printf("%i ", i + 1);
        for (int j = 0; j < 16; j++)
            xil_printf("%i", EK[i] & (1 << (15 - j)) ? 1 : 0);
        xil_printf("\n");
    }
}

uint16_t inv_mod(uint32_t x) {
    uint32_t m = MOD, m0 = m;
    int32_t x0 = 0, x1 = 1;

    if (m == 1) return 0;

    uint32_t q, aux;
    while (x > 1) {
        q = x / m;

        aux = m;
        m = x % m;
        x = aux;

        aux = x0;
        x0 = x1 - q * x0;
        x1 = aux;
    }

    return x1 < 0 ? x1 + m0 : x1;
}

void generate_decrypt_keys() {
    generate_encrypt_keys();

    DK[0] = inv_mod(EK[48]);
    DK[1] = -EK[49];
    DK[2] = -EK[50];
    DK[3] = inv_mod(EK[51]);

    for (int i = 4; i < 52; i += 6) {
        DK[i] = EK[50 - i];
        DK[i + 1] = EK[50 - i + 1];
        DK[i + 2] = inv_mod(EK[50 - i - 4]);
        if (i == 46) {
            DK[i + 3] = -EK[50 - i - 3];
            DK[i + 4] = -EK[50 - i - 2];
        } else {
            DK[i + 3] = -EK[50 - i - 2];
            DK[i + 4] = -EK[50 - i - 3];
        }
        DK[i + 5] = inv_mod(EK[50 - i - 1]);
    }
}

uint16_t mul(uint16_t a, uint16_t b) {
    uint32_t res = a * b;

    if (res == 0)
        return -a - b + 1;
    else {
        uint16_t lo, hi;
        hi = res >> 16;
        lo = res;
        if (lo > hi)
            return lo - hi;
        else
            return lo - hi + 1;
    }
}

void apply_IDEA_block(uint16_t *key) {
    // the first 8 rounds
    for (int i = 0; i < 8; i++) {
        uint16_t x1k1 = mul(msg[0], key[0]);
        uint16_t x2k2 = msg[1] + key[1];
        uint16_t x3k3 = msg[2] + key[2];
        uint16_t x4k4 = mul(msg[3], key[3]);

        uint16_t xor13 = x1k1 ^x3k3;
        uint16_t xor24 = x2k2 ^x4k4;

        uint16_t mul_k5 = mul(xor13, key[4]);
        uint16_t add_k5 = mul_k5 + xor24;

        uint16_t mul_k6 = mul(add_k5, key[5]);
        uint16_t add_k6 = mul_k5 + mul_k6;

        uint16_t xor1 = x1k1 ^mul_k6;
        uint16_t xor2 = x2k2 ^add_k6;
        uint16_t xor3 = x3k3 ^mul_k6;
        uint16_t xor4 = x4k4 ^add_k6;

        msg[0] = xor1;
        msg[1] = xor3;
        msg[2] = xor2;
        msg[3] = xor4;

        key += 6;
    }

    // the last half-round
    msg[0] = mul(msg[0], key[0]);
    uint16_t aux = msg[1];
    msg[1] = msg[2] + key[1];
    msg[2] = aux + key[2];
    msg[3] = mul(msg[3], key[3]);

    // print resulting message
    int p = 0;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 16; j++, p++) {
            if (p % 4 == 0) xil_printf(" ");
            xil_printf("%i", msg[i] & (1 << (15 - j)) ? 1 : 0);
        }
}

void encrypt() {
    generate_encrypt_keys();
    apply_IDEA_block(EK);
}

void decrypt() {
    generate_decrypt_keys();
    apply_IDEA_block(DK);
}

int main() {
	init_platform();

    // parse input message
    char msg_txt[64] = "0000010100110010000010100110010000010100110010000001100111111010";    // decrypted message
    // char msg_txt[64] = "0110010110111110100001111110011110100010010100111000101011101101"; // encrypted message
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 16; j++)
            msg[i] |= (msg_txt[i * 16 + j] == '1' ? 1 : 0) << (15 - j);

    encrypt();
    // decrypt();

    cleanup_platform();

    return 0;
}
