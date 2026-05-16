type ProtocolDirection = "SEND" | "RECV";

export type ProtocolPayloadEvent = {
  direction: ProtocolDirection;
  message: string;
  payloadHex: string;
  payloadPlainHex?: string;
  payloadEncryptedHex?: string;
};

export type ProtocolPayloadRow = {
  key: string;
  value: string;
  tone?: "app" | "kerberos" | "error" | "plain" | "cipher";
};

export type ProtocolPayloadHexBlock = {
  title: string;
  hex: string;
  tone: "plain" | "cipher";
};

export type ProtocolPayloadView = {
  rows: ProtocolPayloadRow[];
  hexBlocks: ProtocolPayloadHexBlock[];
};

const entityLabels = new Map<number, string>([
  [0x01, "Client1"],
  [0x02, "Client2"],
  [0x03, "Client3"],
  [0x04, "Client4"],
  [0x11, "AS"],
  [0x12, "TGS"],
  [0x13, "V"],
]);

const msgLabels = new Map<number, string>([
  [0x01, "MSG_AS_REQ"],
  [0x02, "MSG_AS_REP"],
  [0x03, "MSG_TGS_REQ"],
  [0x04, "MSG_TGS_REP"],
  [0x05, "MSG_V_AUTH_REQ"],
  [0x06, "MSG_V_AUTH_REP"],
  [0x07, "MSG_CERT_C2V"],
  [0x08, "MSG_CERT_V2C"],
  [0x65, "MSG_ERROR"],
  [0x66, "MSG_APP"],
]);

const appLabels = new Map<number, string>([
  [0x01, "KEY_DOWN"],
  [0x02, "KEY_UP"],
  [0x03, "AIM_EVENT"],
  [0x04, "FIRE_EVENT"],
  [0x05, "GAME_JOIN_REQ"],
  [0x06, "GAME_START"],
  [0x07, "GAME_STATE"],
  [0x08, "APP_ACK"],
  [0x09, "GAME_MOVE"],
  [0x0a, "GAME_TARGET"],
  [0x0b, "GAME_SHOOT"],
  [0x0c, "GAME_NAME"],
]);

const errorLabels = new Map<number, string>([
  [0x01, "ERR_PASSWORD_WRONG"],
  [0x02, "ERR_TGT_EXPIRED"],
  [0x03, "ERR_TICKET_V_EXPIRED"],
  [0x04, "ERR_TGS_ID_MISMATCH"],
  [0x05, "ERR_V_ID_MISMATCH"],
  [0x06, "ERR_REPLAY_DETECTED"],
  [0x07, "ERR_UNSUPPORTED_MSG_TYPE"],
]);

const gameLabels = new Map<number, string>([
  [0x01, "JOIN"],
  [0x02, "MOVE"],
  [0x03, "TARGET"],
  [0x04, "SHOOT"],
  [0x05, "NAME"],
  [0x10, "STATE"],
  [0x7f, "ERROR"],
]);

class ByteReader {
  private offset = 0;

  constructor(private readonly bytes: number[]) {}

  remaining() {
    return this.bytes.length - this.offset;
  }

  readU8() {
    if (this.remaining() < 1) {
      throw new Error("short u8");
    }
    return this.bytes[this.offset++];
  }

  readU16() {
    if (this.remaining() < 2) {
      throw new Error("short u16");
    }
    const value = (this.bytes[this.offset] << 8) | this.bytes[this.offset + 1];
    this.offset += 2;
    return value;
  }

  readU32() {
    if (this.remaining() < 4) {
      throw new Error("short u32");
    }
    const value =
      this.bytes[this.offset] * 0x1000000 +
      ((this.bytes[this.offset + 1] << 16) |
        (this.bytes[this.offset + 2] << 8) |
        this.bytes[this.offset + 3]);
    this.offset += 4;
    return value >>> 0;
  }

  readU64Hex() {
    return `0x${this.readBytes(8).map(byteHex).join("")}`;
  }

  readBytes(length: number) {
    if (this.remaining() < length) {
      throw new Error("short bytes");
    }
    const out = this.bytes.slice(this.offset, this.offset + length);
    this.offset += length;
    return out;
  }

  readStringU8() {
    const length = this.readU8();
    return textFromBytes(this.readBytes(length));
  }

  readF32() {
    const bytes = this.readBytes(4);
    const view = new DataView(new Uint8Array(bytes).buffer);
    return view.getFloat32(0, false);
  }
}

export function buildProtocolPayloadView(event: ProtocolPayloadEvent): ProtocolPayloadView {
  const parseHex = event.payloadPlainHex || event.payloadHex;
  return {
    rows: parsePayloadRows(event.message, parseHex),
    hexBlocks: buildHexBlocks(event),
  };
}

export function formatHexString(hex: string) {
  const clean = cleanHex(hex);
  return clean.match(/.{1,2}/g)?.join(" ") ?? "";
}

function buildHexBlocks(event: ProtocolPayloadEvent): ProtocolPayloadHexBlock[] {
  const plain = event.payloadPlainHex ?? "";
  const encrypted = event.payloadEncryptedHex ?? "";
  if (plain && encrypted) {
    const plainBlock = { title: "payload plain hex", hex: plain, tone: "plain" as const };
    const encryptedBlock = {
      title: "payload encrypted hex",
      hex: encrypted,
      tone: "cipher" as const,
    };
    return event.direction === "RECV"
      ? [encryptedBlock, plainBlock]
      : [plainBlock, encryptedBlock];
  }
  return [{ title: "payload hex", hex: event.payloadHex, tone: "plain" }];
}

function parsePayloadRows(message: string, hex: string): ProtocolPayloadRow[] {
  const bytes = hexToBytes(hex);
  if (bytes.length === 0) {
    return [];
  }
  try {
    if (message.startsWith("MSG_APP")) {
      return parseSignedAppRows(bytes);
    }
    if (message === "MSG_AS_REQ") {
      return parseAsReqRows(bytes);
    }
    if (message === "MSG_TGS_REQ") {
      return parseTgsReqRows(bytes);
    }
    if (message === "MSG_V_AUTH_REQ") {
      return parseVAuthReqRows(bytes);
    }
    if (message === "MSG_CERT_C2V") {
      return parseCertC2VRows(bytes);
    }
    if (message.startsWith("MSG_ERROR")) {
      return parseErrorRows(bytes);
    }
  } catch {
    return [{ key: "payload_len", value: `${bytes.length}` }];
  }
  return [{ key: "payload_len", value: `${bytes.length}` }];
}

function parseAsReqRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  return [
    { key: "idc", value: entityLabel(reader.readU8()), tone: "kerberos" },
    { key: "idtgs", value: entityLabel(reader.readU8()), tone: "kerberos" },
    { key: "ts1", value: reader.readU64Hex() },
  ];
}

function parseTgsReqRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  const idv = reader.readU8();
  const ticketLen = reader.readU16();
  const ticket = reader.readBytes(ticketLen);
  const authLen = reader.readU16();
  const authenticator = reader.readBytes(authLen);
  return [
    { key: "idv", value: entityLabel(idv), tone: "kerberos" },
    { key: "ticket_tgs", value: summarizeBytes(ticket) },
    { key: "authenticator_tgs", value: summarizeBytes(authenticator) },
  ];
}

function parseVAuthReqRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  const ticketLen = reader.readU16();
  const ticket = reader.readBytes(ticketLen);
  const authLen = reader.readU16();
  const authenticator = reader.readBytes(authLen);
  return [
    { key: "ticket_v", value: summarizeBytes(ticket) },
    { key: "authenticator_v", value: summarizeBytes(authenticator) },
  ];
}

function parseCertC2VRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  const clientId = reader.readU8();
  const certLen = reader.readU16();
  const cert = reader.readBytes(certLen);
  return [
    { key: "client_id", value: entityLabel(clientId), tone: "kerberos" },
    { key: "cert", value: summarizeBytes(cert) },
  ];
}

function parseErrorRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  const code = reader.readU8();
  return [
    { key: "err_code", value: errorLabels.get(code) ?? `0x${byteHex(code)}`, tone: "error" },
    { key: "err_msg", value: textFromBytes(reader.readBytes(reader.remaining())) },
  ];
}

function parseSignedAppRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  const appCode = reader.readU8();
  const appPayloadLen = reader.readU16();
  const appPayload = reader.readBytes(appPayloadLen);
  const signatureLen = reader.readU16();
  const signature = reader.readBytes(signatureLen);
  const appCodeLabel = appLabels.get(appCode) ?? `0x${byteHex(appCode)}`;
  const rows: ProtocolPayloadRow[] = [
    { key: "app_code", value: appCodeLabel, tone: "app" },
    ...parseAppPayloadRows(appCodeLabel, appPayload),
    { key: "signature", value: summarizeBytes(signature) },
  ];
  return rows;
}

function parseAppPayloadRows(appCode: string, appPayload: number[]): ProtocolPayloadRow[] {
  if (appCode === "APP_ACK") {
    return parseAckRows(appPayload);
  }
  if (appCode.startsWith("GAME_")) {
    return parseGameMessageRows(appPayload);
  }
  return [{ key: "app_payload", value: summarizeBytes(appPayload) }];
}

function parseAckRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  return [
    { key: "acked_msg_type", value: msgLabels.get(reader.readU8()) ?? "UNKNOWN" },
    { key: "acked_app_code", value: appLabels.get(reader.readU8()) ?? "UNKNOWN", tone: "app" },
    { key: "acked_src", value: entityLabel(reader.readU8()) },
    { key: "acked_dst", value: entityLabel(reader.readU8()) },
    { key: "acked_payload_len", value: `${reader.readU32()}` },
    { key: "acked_payload_hash", value: reader.readU64Hex() },
  ];
}

function parseGameMessageRows(bytes: number[]): ProtocolPayloadRow[] {
  const reader = new ByteReader(bytes);
  const type = reader.readU8();
  const payload = reader.readBytes(reader.remaining());
  const typeLabel = gameLabels.get(type) ?? `0x${byteHex(type)}`;
  return [{ key: "game_msg", value: formatGamePayload(typeLabel, payload), tone: "app" }];
}

function formatGamePayload(type: string, payload: number[]) {
  const reader = new ByteReader(payload);
  try {
    if (type === "MOVE") {
      return `MOVE x=${toI8(reader.readU8())} y=${toI8(reader.readU8())}`;
    }
    if (type === "TARGET") {
      return `TARGET angle=${reader.readF32().toFixed(3)}`;
    }
    if (type === "SHOOT") {
      return `SHOOT shooting=${reader.readU8() !== 0}`;
    }
    if (type === "JOIN") {
      return `JOIN client=${entityLabel(reader.readU8())} name=${reader.readStringU8()}`;
    }
    if (type === "NAME") {
      return `NAME ${reader.readStringU8()}`;
    }
    if (type === "STATE") {
      const serverTime = reader.readU64Hex();
      const totalScore = reader.readU16();
      const winnerTeam = toI8(reader.readU8());
      return `STATE server_time=${serverTime} total_score=${totalScore} winner_team=${winnerTeam}`;
    }
  } catch {
    return `${type} ${summarizeBytes(payload)}`;
  }
  return `${type} ${summarizeBytes(payload)}`;
}

function hexToBytes(hex: string) {
  const clean = cleanHex(hex);
  const bytes: number[] = [];
  for (let i = 0; i + 1 < clean.length; i += 2) {
    bytes.push(Number.parseInt(clean.slice(i, i + 2), 16));
  }
  return bytes;
}

function cleanHex(hex: string) {
  return hex.replace(/^0x/i, "").replace(/\s+/g, "").toLowerCase();
}

function summarizeBytes(bytes: number[]) {
  const shown = bytes.slice(0, 16).map(byteHex).join(" ");
  return bytes.length > 16 ? `${shown} ...` : shown;
}

function byteHex(value: number) {
  return value.toString(16).padStart(2, "0");
}

function entityLabel(value: number) {
  return entityLabels.get(value) ?? `0x${byteHex(value)}`;
}

function textFromBytes(bytes: number[]) {
  return new TextDecoder().decode(new Uint8Array(bytes));
}

function toI8(value: number) {
  return value > 127 ? value - 256 : value;
}
