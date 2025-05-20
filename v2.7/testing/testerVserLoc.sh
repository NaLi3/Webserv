#!/bin/sh

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, HOST NOT DEFINED (TARGET main) ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, HOST NAME 'doesnotexist.com' (TARGET main) ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:doesnotexist.com:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, HOST NAME 'main.com' (TARGET main) ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:main.com:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, HOST NOT DEFINED, PORT 8081 (TARGET side1) ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:localhost:8081\r\n\r\n" | nc -N 127.0.0.1 8081

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, HOST NOT DEFINED, IP LOCAL (TARGET side2) ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:10.12.5.11:8080\r\n\r\n" | nc -N 10.12.5.11 8080

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, HOST NAME 'side3.com' (TARGET side3) ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:side3.com:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE, TO '/site/secretplace' (TARGET LOCATION secretplace) ===\033[0m\n"
printf "GET /site/secretplace/welcomepage.html HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
