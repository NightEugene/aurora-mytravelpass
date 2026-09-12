#!/usr/bin/env python3
"""Клиент MCP-сервера портала разработчиков ОС Аврора.

Сервер: https://developer.auroraos.ru/api/mcp (имя сервера «dev-aurora»,
транспорт Streamable HTTP, протокол MCP 2025-03-26, аутентификация не
требуется — нужен только браузерный User-Agent).

Использование:
    aurora_mcp.py tools                          — список инструментов
    aurora_mcp.py call <имя> '<json-аргументы>'  — вызвать инструмент

Примеры:
    aurora_mcp.py call search '{"query": "nfcd Tag Transceive"}'
    aurora_mcp.py call search_code '{"query": "AuroraApp main.cpp"}'
    aurora_mcp.py call get_document '{"id": "..."}'
"""

import json
import sys
from urllib.request import Request, urlopen

URL = "https://developer.auroraos.ru/api/mcp"
HEADERS = {
    # WAF портала отклоняет небраузерные User-Agent (curl и т.п.)
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                  "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36",
    "Content-Type": "application/json",
    "Accept": "application/json, text/event-stream",
}


def rpc(method, params, session=None, request_id=None):
    """Выполняет JSON-RPC вызов. Возвращает (result, session_id)."""
    body = {"jsonrpc": "2.0", "method": method, "params": params}
    if request_id is not None:
        body["id"] = request_id
    request = Request(URL, data=json.dumps(body).encode(), headers=HEADERS)
    if session:
        request.add_header("Mcp-Session-Id", session)
    with urlopen(request, timeout=60) as response:
        session = response.headers.get("Mcp-Session-Id", session)
        raw = response.read().decode()
    if not raw.strip():
        return {}, session
    # Ответ может прийти в формате SSE — достаём JSON из строк «data:»
    if raw.lstrip().startswith(("event:", "data:")):
        raw = "\n".join(
            line[len("data:"):].strip()
            for line in raw.splitlines()
            if line.startswith("data:")
        )
    return json.loads(raw), session


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("tools", "call"):
        sys.exit(__doc__)
    _, session = rpc("initialize", {
        "protocolVersion": "2025-03-26",
        "capabilities": {},
        "clientInfo": {"name": "podorozhnik-balance-dev", "version": "0.1"},
    }, request_id=1)
    rpc("notifications/initialized", {}, session)

    if sys.argv[1] == "tools":
        result, _ = rpc("tools/list", {}, session, request_id=2)
        print(json.dumps(result.get("result", result), ensure_ascii=False, indent=2))
        return

    name = sys.argv[2]
    arguments = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
    result, _ = rpc("tools/call", {"name": name, "arguments": arguments},
                    session, request_id=2)
    content = result.get("result", {}).get("content", [])
    texts = [c["text"] for c in content if c.get("type") == "text"]
    print("\n".join(texts) if texts
          else json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
