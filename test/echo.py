__author__ = 'yaocoder'
from socket import *

TOKEN_LENGTH = 5
TOKEN_STR = "12345"

sfd = socket(AF_INET, SOCK_STREAM)

ip = '192.168.14.234'
port = 12006
sfd.connect((ip, port))

ch = ['yaocoder', 'wht', 'xty', 'zfd']
for i in ch:
    message = TOKEN_STR + 'hello world!' + i + '\r\n'
    sfd.send(message)
    data = sfd.recv(20)
    print 'the data received is ',data
else:
    print 'done!'
