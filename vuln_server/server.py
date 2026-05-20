import http.server
import socketserver
import json
import urllib.parse
import os

PORT = 8081
STATUS_FILE = os.path.join(os.path.dirname(__file__), "status.txt")

class VulnHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/status":
            query = urllib.parse.parse_qs(parsed.query)
            version = query.get("version", ["unknown"])[0]
            
            # Default to SAFE
            state = "SAFE"
            cve = ""
            msg = "System is fully secure."
            
            # Read from status file to simulate remote triggers
            if os.path.exists(STATUS_FILE):
                with open(STATUS_FILE, "r") as f:
                    state = f.read().strip().upper()
            
            if state == "IMMEDIATE":
                cve = "CVE-2026-9999"
                msg = f"Critical remote code execution in version {version}."
            elif state == "CLOSE":
                cve = "CVE-2026-8888"
                msg = f"Denial of service in version {version}."
                
            response = {
                "status": state,
                "cve": cve,
                "message": msg
            }
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(response).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

if __name__ == "__main__":
    with socketserver.TCPServer(("", PORT), VulnHandler) as httpd:
        print(f"Vulnerability Server running on port {PORT}")
        with open(STATUS_FILE, "w") as f:
            f.write("SAFE")
        httpd.serve_forever()
