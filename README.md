## Project Files
- `orderbook.cpp` - main orderbook engine + CLI entrypoint
- `Order.h`, `OrderType.h`, `Side.h` - core order model/types
- `orderbook_build.exe` - compiled binary (generated after build)
- `server.py` - HTTP server that connects web UI to C++ process
- `index.html`, `styles.css`, `app.js` - web frontend
## Build
From project root:
```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic "orderbook.cpp" -o "orderbook_build.exe"
```
## Run (CLI)
```powershell
.\orderbook_build.exe
```
CLI commands:
- `add <id> <buy|sell> <qty> <gtc|fak|gfd|mkt> <price>`
- `modify <id> <buy|sell> <qty> <price>`
- `cancel <id>`
- `book`
- `help`
- `exit`
For `mkt`, pass `0` as price.
## Run (Web UI)
1. Start server:
```powershell
python server.py
```
2. Open browser:
- `http://127.0.0.1:8080`
The server starts the C++ binary in API mode and routes UI actions to it.
