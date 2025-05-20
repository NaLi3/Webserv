#!/bin/sh

echo "\n\n\033[33m=== GET REQUEST ON MAIN PAGE ===\033[0m\n"
printf "GET /site/welcomepage.html HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON PHP PAGE ===\033[0m\n"
printf "GET /app/dynpage.php HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== POST CGI FOR FORM ===\033[0m\n"
printf "POST /app/handleform.py HTTP/1.1\r\nHost:localhost:8080\r\nContent-Length:17\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\nname=aa&surname=b" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON LOCATION SITE (DIRECTORY REDIRECTION) ===\033[0m\n"
printf "GET /site HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
printf "GET /site/ HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON ROOT ===\033[0m\n"
printf "GET / HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== GET REQUEST ON REDIRECTED PATH ===\033[0m\n"
printf "GET /oldsite/welcomepage.html HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== POST UPLOAD ON LOCATION SITE - RAW FILE ===\033[0m\n"
printf "POST /site/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\nContent-Length:3\r\nContent-Type:text/plain\r\n\r\nabc" | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== GET REQUEST ON UPLOADED FILE ===\033[0m\n"
printf "GET /uploads/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== DELETE REQUEST ON UPLOADED FILE ===\033[0m\n"
printf "DELETE /uploads/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== POST UPLOAD ON LOCATION SITE - MULTIPART BODY ===\033[0m\n"
printf 'POST /site/ HTTP/1.1\r\nHost:localhost:8080\r\nContent-Length:217\r\nContent-Type:multipart/form-data; boundary=+++\r\n\r\n--+++\r\nContent-Disposition: form-data; name="file1"; filename="m1.txt"\r\nContent-Type: text/plain\r\n\r\nA\nB\nC\n--+++\r\nContent-Disposition: form-data; name="file2"; filename="m2.txt"\r\nContent-Type: text/plain\r\n\r\n1\n2\n3\n--+++' | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== GET REQUEST ON UPLOADED FILES ===\033[0m\n"
printf "GET /uploads/m1.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
printf "GET /uploads/m2.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== DELETE REQUEST ON UPLOADED FILES ===\033[0m\n"
printf "DELETE /uploads/m1.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
printf "DELETE /uploads/m2.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== POST UPLOAD ON LOCATION SITE - RAW FILE, CHUNKED ===\033[0m\n"
printf "POST /site/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\nContent-Type:text/plain\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabcde\r\n2\r\nf\n\r\n4\r\npol\n\r\n0\r\n\r\n" | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== GET REQUEST ON UPLOADED FILE ===\033[0m\n"
printf "GET /uploads/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== DELETE REQUEST ON UPLOADED FILE ===\033[0m\n"
printf "DELETE /uploads/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== POST CGI FOR FORM, CHUNKED ===\033[0m\n"
printf "POST /app/handleform.py HTTP/1.1\r\nHost:localhost:8080\r\nContent-Type:application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nna\r\nB\r\nme=antoine&\r\nD\r\nsurname=bedin\r\n0\r\n\r\n" | nc -N 127.0.0.1 8080

echo "\n\n\033[33m=== {CURL} POST UPLOAD ON LOCATION SITE - CHUNKED, MULTIPART BODY ===\033[0m\n"
curl -v http://localhost:8080/site/ -F "file=@loremFiles/lorem3.txt" -H "Transfer-Encoding: chunked"
echo "\n\n\033[33m=== GET REQUEST ON UPLOADED FILE ===\033[0m\n"
printf "GET /uploads/lorem3.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
echo "\n\n\033[33m=== DELETE REQUEST ON UPLOADED FILE ===\033[0m\n"
printf "DELETE /uploads/lorem3.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc -N 127.0.0.1 8080
