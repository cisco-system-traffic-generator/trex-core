#!/usr/bin/python

'''
Designed to work with OpenSSL v1.1.0f
'''


from ctypes import CDLL, PyDLL, c_void_p, CFUNCTYPE, c_int, c_buffer, sizeof, byref, c_char_p, c_ulong, c_long, Structure, cast, pointer, addressof, POINTER
import os
import platform
import sys

cur_dir = os.path.abspath(os.path.dirname(__file__))

if platform.uname()[0] == "Darwin":
    lib_crypto_name = 'libcrypto.1.1.dylib'
    lib_ssl_name = 'libssl.1.1.dylib'
    lib_c_name = "libc.dylib"
else:
    lib_crypto_name = 'libcrypto.so.1.1'
    lib_ssl_name = 'libssl.so.1.1'
    lib_c_name = "libc.so.6"

try:
    libcrypto = CDLL(lib_crypto_name)
    libssl    = CDLL(lib_ssl_name)
except OSError:
    # use local
    libcrypto = CDLL(os.path.join(cur_dir, lib_crypto_name))
    libssl    = CDLL(os.path.join(cur_dir, lib_ssl_name))
libc = CDLL(lib_c_name)


###################
# libssl C methods
###################

libssl.DTLSv1_method.argtypes = []
libssl.DTLSv1_method.restype = c_void_p
libssl.SSL_CTX_free.argtypes = [c_void_p]
libssl.SSL_CTX_new.argtypes = [c_void_p]
libssl.SSL_CTX_new.restype = c_void_p
libssl.SSL_CTX_set_options.argtypes = [c_void_p, c_ulong]
libssl.SSL_CTX_set_options.restype = c_ulong
libssl.SSL_CTX_use_PrivateKey.argtypes = [c_void_p, c_void_p]
libssl.SSL_CTX_use_RSAPrivateKey.argtypes = [c_void_p, c_void_p]
libssl.SSL_SESSION_free.argtypes = []
libssl.SSL_SESSION_free.restype = c_void_p
libssl.SSL_alert_desc_string_long.restype = c_char_p
libssl.SSL_alert_type_string_long.restype = c_char_p
libssl.SSL_check_private_key.argtypes = [c_void_p]
libssl.SSL_clear.argtypes = [c_void_p]
libssl.SSL_do_handshake.argtypes = [c_void_p]
libssl.SSL_free.argtypes = [c_void_p]
libssl.SSL_get_error.argtypes = [c_void_p, c_int]
libssl.SSL_get_shutdown.argtypes = [c_void_p]
libssl.SSL_is_init_finished.argtypes = [c_void_p]
libssl.SSL_new.argtypes = [c_void_p]
libssl.SSL_new.restype = c_void_p
libssl.SSL_read.argtypes = [c_void_p, c_void_p, c_int]
libssl.SSL_set_bio.argtypes = [c_void_p, c_void_p, c_void_p]
libssl.SSL_set_connect_state.argtypes = [c_void_p]
libssl.SSL_set_info_callback.argtypes = [c_void_p, c_void_p]
libssl.SSL_set_msg_callback.argtypes = [c_void_p, c_void_p]
libssl.SSL_shutdown.argtypes = [c_void_p]
libssl.SSL_state_string.argtypes = [c_void_p]
libssl.SSL_state_string.restype = c_char_p
libssl.SSL_state_string_long.argtypes = [c_void_p]
libssl.SSL_state_string_long.restype = c_char_p
libssl.SSL_use_certificate.argtypes = [c_void_p, c_void_p]
libssl.SSL_use_certificate_file.argtypes = [c_void_p, c_char_p, c_int]
libssl.SSL_use_PrivateKey_file.argtypes = [c_void_p, c_char_p, c_int]
libssl.SSL_write.argtypes = [c_void_p, c_void_p, c_int]
libssl.SSL_ctrl.argtypes = [c_void_p, c_int, c_long, c_void_p]
libssl.SSL_get_state.argtypes = [c_void_p]
libssl.ASN1_INTEGER_free.argtypes = [c_void_p]
libssl.ASN1_INTEGER_new.argtypes = []
libssl.ASN1_INTEGER_new.restype = c_void_p
libssl.ASN1_INTEGER_set.argtypes = [c_void_p, c_int]
libssl.ASN1_STRING_set_default_mask_asc.argtypes = [c_char_p]

class ssl_timeval(Structure):
    _fields_ = [("tv_sec", c_long), ("tv_usec", c_long)]

def DTLSv1_handle_timeout(ssl):
    libssl.SSL_ctrl(ssl, SSL_CONST.DTLS_CTRL_HANDLE_TIMEOUT, 0, 0)
libssl.DTLSv1_handle_timeout = DTLSv1_handle_timeout

def DTLSv1_get_timeout(ssl):
    v = ssl_timeval(0, 0)
    pv = pointer(v)
    libssl.SSL_ctrl(ssl, SSL_CONST.DTLS_CTRL_GET_TIMEOUT, 0, cast(pv, c_void_p))
    return v
libssl.DTLSv1_get_timeout = DTLSv1_get_timeout

######################
# libcrypto C methods
######################

libcrypto.BIO_ctrl_pending.argtypes = [c_void_p]
libcrypto.BIO_ctrl_wpending.argtypes = [c_void_p]
libcrypto.BIO_new.argtypes = [c_void_p]
libcrypto.BIO_new.restype = c_void_p
libcrypto.BIO_new_fp.argtypes = [c_void_p, c_int]
libcrypto.BIO_new_fp.restype = c_void_p
libcrypto.BIO_read.argtypes = [c_void_p, c_void_p, c_int]
libcrypto.BIO_s_mem.restype = c_void_p
libcrypto.BIO_test_flags.argtypes = [c_void_p, c_int]
libcrypto.BIO_write.argtypes = [c_void_p, c_void_p, c_int]
libcrypto.BN_free.argtypes = [c_void_p]
libcrypto.BN_new.restype = c_void_p
libcrypto.BN_set_word.argtypes = [c_void_p, c_ulong]
libcrypto.ERR_get_error.argtypes = []
libcrypto.ERR_get_error.restype = c_ulong
libcrypto.ERR_reason_error_string.argtypes = [c_ulong]
libcrypto.ERR_reason_error_string.restype = c_char_p
libcrypto.EVP_sha256.restype = c_void_p
libcrypto.EVP_PKEY_free.argtypes = [c_void_p]
libcrypto.EVP_PKEY_new.restype = c_void_p
libcrypto.EVP_PKEY_set1_RSA.argtypes = [c_void_p, c_void_p]
libcrypto.PEM_read_bio_PrivateKey.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p]
libcrypto.PEM_read_bio_PrivateKey.restype = c_void_p
libcrypto.PEM_write_bio_X509.argtypes = [c_void_p, c_void_p]
libcrypto.RSA_free.argtypes = [c_void_p]
libcrypto.RSA_generate_key_ex.argtypes = [c_void_p, c_int, c_void_p, c_void_p]
libcrypto.RSA_new.restype = c_void_p
libcrypto.PEM_read_RSAPrivateKey.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p]
libcrypto.PEM_read_RSAPrivateKey.restype = c_void_p
libcrypto.X509_NAME_add_entry_by_txt.argtypes = [c_void_p, c_char_p, c_int, c_char_p, c_int, c_int, c_int]
libcrypto.X509_NAME_free.argtypes = [c_void_p]
libcrypto.X509_NAME_new.restype = c_void_p
libcrypto.X509_free.argtypes = [c_void_p]
libcrypto.X509_getm_notAfter.argtypes = [c_void_p]
libcrypto.X509_getm_notAfter.restype = c_void_p
libcrypto.X509_getm_notBefore.argtypes = [c_void_p]
libcrypto.X509_getm_notBefore.restype = c_void_p
libcrypto.X509_new.argtypes = []
libcrypto.X509_new.restype = c_void_p
libcrypto.X509_set_issuer_name.argtypes = [c_void_p, c_void_p]
libcrypto.X509_set_pubkey.argtypes = [c_void_p, c_void_p]
libcrypto.X509_set_subject_name.argtypes = [c_void_p, c_void_p]
libcrypto.X509_set_version.argtypes = [c_void_p, c_long]
libcrypto.X509_sign.argtypes = [c_void_p, c_void_p, c_void_p]
libcrypto.X509_time_adj_ex.argtypes = [c_void_p, c_int, c_long, c_void_p]
libcrypto.X509_time_adj_ex.restype = c_void_p
libcrypto.X509_print_fp.argtypes = [c_void_p, c_void_p]
libcrypto.X509_set_serialNumber.argtypes = [c_void_p, c_void_p]
libcrypto.PEM_write_bio_X509.argtypes = [c_void_p, c_void_p]

def PEM_write_bio_X509_to_file(x509, file):
    fopen = libc.fopen
    fopen.argtypes = c_char_p, c_char_p,
    fopen.restype = c_void_p

    fclose = libc.fclose
    fclose.argtypes = c_void_p,
    fclose.restype = c_int

    fp = fopen(file, b'wb')

    outbio = libcrypto.BIO_new_fp(fp, 0)
    ret = libcrypto.PEM_write_bio_X509(outbio, x509)
    fclose(fp)
    return ret

libcrypto.PEM_write_bio_X509_to_file = PEM_write_bio_X509_to_file

def helper_libcrypto_PEM_read_RSAPrivateKey(f):
    rsa_ptr = POINTER(c_void_p)()
    fopen = libc.fopen
    fopen.argtypes = [c_char_p, c_char_p]
    fopen.restype = c_void_p

    fclose = libc.fclose
    fclose.argtypes = [c_void_p]
    fclose.restype = c_int
    if sys.version_info >= (3, 0):
        fp = fopen(bytes(f, 'utf-8'), b'rb')
    else:
        fp = fopen(f, b'rb')
    if not fp:
        return None
    addr_rsa = addressof(rsa_ptr)
    ret = libcrypto.PEM_read_RSAPrivateKey(fp, addr_rsa, None, None)
    fclose(fp)
    return rsa_ptr
libcrypto.PEM_read_RSAPrivateKey_helper = helper_libcrypto_PEM_read_RSAPrivateKey

def helper_libcrypto_X509_print_fp(x509, file):
    fopen = libc.fopen
    fopen.argtypes = c_char_p, c_char_p,
    fopen.restype = c_void_p

    fclose = libc.fclose
    fclose.argtypes = c_void_p,
    fclose.restype = c_int
    fp = fopen(file, b'wb')
    if not fp:
        return -1
    ret = libcrypto.X509_print_fp(fp, x509)
    fclose(fp)
    return ret
libcrypto.X509_print_fp_helper = helper_libcrypto_X509_print_fp

def X509_set_serialNumber_helper(x509, serial):
    
    aserial=libssl.ASN1_INTEGER_new()
    libssl.ASN1_INTEGER_set(aserial, serial)
    ret = libcrypto.X509_set_serialNumber(x509, aserial)
    libssl.ASN1_INTEGER_free(aserial)
    return ret
libcrypto.X509_set_serialNumber_helper = X509_set_serialNumber_helper


##################
# Constants
##################

class SSL_CONST:
    ssl_err = {
        0:  'SSL_ERROR_NONE',
        1:  'SSL_ERROR_SSL',
        2:  'SSL_ERROR_WANT_READ',
        3:  'SSL_ERROR_WANT_WRITE',
        6:  'SSL_ERROR_ZERO_RETURN',
        7:  'SSL_ERROR_WANT_CONNECT',
        8:  'SSL_ERROR_WANT_ACCEPT',
        9:  'SSL_ERROR_WANT_ASYNC',
        10: 'SSL_ERROR_WANT_ASYNC_JOB',
        11: 'SSL_ERROR_WANT_EARLY',
        }

    #SSL_MODE_AUTO_RETRY = 4
    SSL_FILETYPE_PEM = 1

    SSL_ST_CONNECT         = 0x1000
    SSL_ST_ACCEPT          = 0x2000
    SSL_ST_MASK            = 0x0FFF
    SSL_CB_LOOP            = 0x01
    SSL_CB_EXIT            = 0x02
    SSL_CB_READ            = 0x04
    SSL_CB_WRITE           = 0x08
    SSL_CB_ALERT           = 0x4000
    SSL_CB_READ_ALERT      = SSL_CB_ALERT | SSL_CB_READ
    SSL_CB_WRITE_ALERT     = SSL_CB_ALERT | SSL_CB_WRITE
    SSL_CB_ACCEPT_LOOP     = SSL_ST_ACCEPT | SSL_CB_LOOP
    SSL_CB_ACCEPT_EXIT     = SSL_ST_ACCEPT | SSL_CB_EXIT
    SSL_CB_CONNECT_LOOP    = SSL_ST_CONNECT | SSL_CB_LOOP
    SSL_CB_CONNECT_EXIT    = SSL_ST_CONNECT | SSL_CB_EXIT
    SSL_CB_HANDSHAKE_START = 0x10
    SSL_CB_HANDSHAKE_DONE  = 0x20

    DTLS_CTRL_GET_TIMEOUT    = 73
    DTLS_CTRL_HANDLE_TIMEOUT = 74

    BIO_FLAGS_READ         = 0x01
    BIO_FLAGS_WRITE        = 0x02
    BIO_FLAGS_IO_SPECIAL   = 0x04
    BIO_FLAGS_RWS          = BIO_FLAGS_READ | BIO_FLAGS_WRITE | BIO_FLAGS_IO_SPECIAL
    BIO_FLAGS_SHOULD_RETRY = 0x08

    MBSTRING_FLAG          = 0x1000
    MBSTRING_UTF8          = MBSTRING_FLAG
    MBSTRING_ASC           = MBSTRING_FLAG | 1
    MBSTRING_BMP           = MBSTRING_FLAG | 2
    MBSTRING_UNIV          = MBSTRING_FLAG | 4
    RSA_F4                 = 0x10001
    SSL_OP_NO_TICKET       = 0x00004000

    BIO_CTRL_SET_CALLBACK  = 14

