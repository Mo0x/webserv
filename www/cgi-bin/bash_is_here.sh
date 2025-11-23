#!/bin/bash
echo "Content-Type: text/html"
echo

echo "<html><body>"
echo "<h1>Hello from Bash CGI</h1>"
echo "<p>QUERY_STRING = $QUERY_STRING</p>"
echo "</body></html>"