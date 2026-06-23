#include "libmin.h"

typedef unsigned long long u64;

/* Moltiplicazione modulare sicura: (a * b) % m senza overflow */
static u64 mulmod(u64 a, u64 b, u64 m) {
    u64 result = 0;
    a %= m;
    while (b > 0) {
        if (b & 1)
            result = (result + a) % m;
        a = (a + a) % m;
        b >>= 1;
    }
    return result;
}

static u64 mod_pow(u64 base, u64 exp, u64 mod) {
    u64 result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1)
            result = mulmod(result, base, mod);
        exp >>= 1;
        base = mulmod(base, base, mod);
    }
    return result;
}

static u64 mod_inverse(u64 a, u64 mod) {
    u64 m0 = mod;
    long long x0 = 0, x1 = 1, q, t;
    if (mod == 1) return 0;
    while (a > 1) {
        q = a / mod;
        t = mod; mod = a % mod; a = t;
        t = x0; x0 = x1 - q * x0; x1 = t;
    }
    if (x1 < 0) x1 += m0;
    return (u64)x1;
}

static void print_hex_u64(u64 n) {
    if (n == 0) { libmin_printf("0x0"); return; }
    char buf[17]; buf[16] = '\0';
    int i;
    for (i = 15; i >= 0; i--) {
        buf[i] = "0123456789abcdef"[n & 0xF];
        n >>= 4;
    }
    int j = 0;
    while (buf[j] == '0' && buf[j+1] != '\0') j++;
    libmin_printf("0x%s", &buf[j]);
}

int main(void) {
    u64 p = 1000003ULL;
    u64 q = 1000033ULL;
    u64 n = p * q;
    u64 phi = (p - 1) * (q - 1);
    u64 e = 65537;
    u64 d = mod_inverse(e, phi);
    u64 message = 0xdeadbeef;
    u64 encrypted = mod_pow(message, e, n);
    u64 decrypted = mod_pow(encrypted, d, n);

    libmin_printf("RSA Key Pair Example (Hexadecimal Output):\n\n");
    libmin_printf("p: "); print_hex_u64(p);
    libmin_printf("\nq: "); print_hex_u64(q);
    libmin_printf("\nn (modulus): "); print_hex_u64(n);
    libmin_printf("\nphi(n): "); print_hex_u64(phi);
    libmin_printf("\n\nPublic exponent (e): "); print_hex_u64(e);
    libmin_printf("\nPrivate exponent (d): "); print_hex_u64(d);
    libmin_printf("\n\nOriginal message: "); print_hex_u64(message);
    libmin_printf("\nEncrypted message: "); print_hex_u64(encrypted);
    libmin_printf("\nDecrypted message: "); print_hex_u64(decrypted);
    libmin_printf("\n");
    libmin_success();
    return 0;
}