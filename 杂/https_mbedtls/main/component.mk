#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# embed files from the "certs" directory as binary data symbols 将“certs”目录中的文件作为二进制数据符号嵌入
# in the app
COMPONENT_EMBED_TXTFILES := server_root_cert.pem


