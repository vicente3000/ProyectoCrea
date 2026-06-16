const serialStatus = document.querySelector("#serialStatus");
const socketStatus = document.querySelector("#socketStatus");
const connectSerialBtn = document.querySelector("#connectSerialBtn");
const disconnectSerialBtn = document.querySelector("#disconnectSerialBtn");
const baudRateInput = document.querySelector("#baudRate");
const wifiForm = document.querySelector("#wifiForm");
const sendWifiBtn = document.querySelector("#sendWifiBtn");
const ssidInput = document.querySelector("#ssid");
const passwordInput = document.querySelector("#password");
const socketForm = document.querySelector("#socketForm");
const socketUrlInput = document.querySelector("#socketUrl");
const connectSocketBtn = document.querySelector("#connectSocketBtn");
const disconnectSocketBtn = document.querySelector("#disconnectSocketBtn");
const sendMessageForm = document.querySelector("#sendMessageForm");
const outgoingMessageInput = document.querySelector("#outgoingMessage");
const sendSerialMessageBtn = document.querySelector("#sendSerialMessageBtn");
const sendSocketMessageBtn = document.querySelector("#sendSocketMessageBtn");
const clearLogBtn = document.querySelector("#clearLogBtn");
const messageLog = document.querySelector("#messageLog");

let port;
let reader;
let writer;
let readLoopActive = false;
let socket;

const savedSocketUrl = localStorage.getItem("esp32-socket-url");
const savedSsid = localStorage.getItem("esp32-ssid");

if (savedSocketUrl) socketUrlInput.value = savedSocketUrl;
if (savedSsid) ssidInput.value = savedSsid;

function setSerialConnected(connected) {
  serialStatus.classList.toggle("connected", connected);
  serialStatus.lastChild.textContent = connected ? " USB conectado" : " USB desconectado";
  connectSerialBtn.disabled = connected;
  disconnectSerialBtn.disabled = !connected;
  sendWifiBtn.disabled = !connected;
  sendSerialMessageBtn.disabled = !connected;
}

function setSocketConnected(connected) {
  socketStatus.classList.toggle("connected", connected);
  socketStatus.lastChild.textContent = connected ? " WebSocket conectado" : " WebSocket desconectado";
  connectSocketBtn.disabled = connected;
  disconnectSocketBtn.disabled = !connected;
  sendSocketMessageBtn.disabled = !connected;
}

function logMessage(source, payload) {
  const item = document.createElement("li");
  item.className = "message";

  const time = document.createElement("small");
  time.textContent = `${new Date().toLocaleTimeString()} ${source}`;

  const body = document.createElement("pre");
  body.textContent = formatPayload(payload);

  item.append(time, body);
  messageLog.append(item);
  messageLog.scrollTop = messageLog.scrollHeight;
}

function formatPayload(payload) {
  if (typeof payload !== "string") return String(payload);

  try {
    return JSON.stringify(JSON.parse(payload), null, 2);
  } catch {
    return payload;
  }
}

function toLine(data) {
  const value = typeof data === "string" ? data : JSON.stringify(data);
  return value.endsWith("\n") ? value : `${value}\n`;
}

async function writeSerial(data) {
  if (!writer) throw new Error("USB no conectado");
  await writer.write(new TextEncoder().encode(toLine(data)));
}

connectSerialBtn.addEventListener("click", async () => {
  if (!("serial" in navigator)) {
    logMessage("app", "Tu navegador no soporta Web Serial. Usa Chrome o Edge en localhost.");
    return;
  }

  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: Number(baudRateInput.value) });
    writer = port.writable.getWriter();
    setSerialConnected(true);
    logMessage("usb", "Puerto serial conectado");
    readFromSerial();
  } catch (error) {
    logMessage("usb error", error.message);
    await closeSerial();
  }
});

disconnectSerialBtn.addEventListener("click", closeSerial);

async function readFromSerial() {
  readLoopActive = true;
  const decoder = new TextDecoderStream();
  const readableClosed = port.readable.pipeTo(decoder.writable);
  reader = decoder.readable.getReader();

  try {
    while (readLoopActive) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value) logMessage("usb", value.trim());
    }
  } catch (error) {
    if (readLoopActive) logMessage("usb error", error.message);
  } finally {
    reader?.releaseLock();
    await readableClosed.catch(() => {});
  }
}

async function closeSerial() {
  readLoopActive = false;

  try {
    await reader?.cancel();
  } catch {}

  try {
    writer?.releaseLock();
    writer = null;
    await port?.close();
  } catch (error) {
    logMessage("usb error", error.message);
  } finally {
    port = null;
    setSerialConnected(false);
  }
}

wifiForm.addEventListener("submit", async (event) => {
  event.preventDefault();

  const payload = {
    type: "wifi",
    ssid: ssidInput.value.trim(),
    password: passwordInput.value
  };

  if (!payload.ssid) return;

  try {
    localStorage.setItem("esp32-ssid", payload.ssid);
    await writeSerial(payload);
    logMessage("app", `WiFi enviado: ${payload.ssid}`);
  } catch (error) {
    logMessage("usb error", error.message);
  }
});

socketForm.addEventListener("submit", (event) => {
  event.preventDefault();
  connectSocket();
});

function connectSocket() {
  const url = socketUrlInput.value.trim();
  if (!url) return;

  try {
    socket = new WebSocket(url);
    localStorage.setItem("esp32-socket-url", url);
    logMessage("ws", `Conectando a ${url}`);
  } catch (error) {
    logMessage("ws error", error.message);
    return;
  }

  socket.addEventListener("open", () => {
    setSocketConnected(true);
    logMessage("ws", "Conexion abierta");
  });

  socket.addEventListener("message", (event) => {
    logMessage("ws", event.data);
  });

  socket.addEventListener("close", () => {
    setSocketConnected(false);
    logMessage("ws", "Conexion cerrada");
  });

  socket.addEventListener("error", () => {
    logMessage("ws error", "No se pudo mantener la conexion");
  });
}

disconnectSocketBtn.addEventListener("click", () => {
  socket?.close();
  socket = null;
  setSocketConnected(false);
});

sendMessageForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const message = outgoingMessageInput.value.trim();
  if (!message || socket?.readyState !== WebSocket.OPEN) return;
  socket.send(message);
  logMessage("app -> ws", message);
});

sendSerialMessageBtn.addEventListener("click", async () => {
  const message = outgoingMessageInput.value.trim();
  if (!message) return;

  try {
    await writeSerial(message);
    logMessage("app -> usb", message);
  } catch (error) {
    logMessage("usb error", error.message);
  }
});

clearLogBtn.addEventListener("click", () => {
  messageLog.replaceChildren();
});

setSerialConnected(false);
setSocketConnected(false);
logMessage("app", "Listo. Conecta el ESP32 por USB o por WebSocket.");
