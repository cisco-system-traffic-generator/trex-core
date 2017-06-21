wget https://www.openssl.org/source/openssl-1.1.0f.tar.gz
tar -xzf openssl-1.1.0f.tar.gz
cd openssl-1.1.0f
./config
make
cp libssl.so.1.1 ..
cp libcrypto.so.1.1 ..

