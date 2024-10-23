#!/bin/bash

# 设置证书和私钥的路径
CERT_PATH="/etc/ssl/certs/server.crt"
KEY_PATH="/etc/ssl/certs/server.key"

# 生成私钥
openssl genrsa -out $KEY_PATH 2048

# 生成自签名证书
openssl req -new -x509 -key $KEY_PATH -out $CERT_PATH -days 365 -subj "/C=CN/ST=Beijing/L=Beijing/O=MyCompany/CN=example.com"

echo "证书和私钥已生成："
echo "证书路径: $CERT_PATH"
echo "私钥路径: $KEY_PATH"