import { buildProtocolPayloadView } from "../src/protocolPayload";

function assertCondition(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

function rowValue(rows: ReturnType<typeof buildProtocolPayloadView>["rows"], key: string) {
  return rows.find((row) => row.key === key)?.value;
}

function testEncryptedOuterPayloadIsNotParsedAsPlain() {
  const fakeCipherThatLooksLikeAsRep =
    "000102030405060711000000000000000100000000000000020000";
  const view = buildProtocolPayloadView({
    direction: "RECV",
    message: "MSG_AS_REP",
    payloadHex: fakeCipherThatLooksLikeAsRep,
  });

  assertCondition(rowValue(view.rows, "payload_state") === "encrypted", "encrypted AS_REP parsed as plain");
  assertCondition(view.hexBlocks[0]?.title === "payload encrypted hex", "encrypted AS_REP hex mislabeled");
}

function testPlainAsRepShowsActualSessionKeyBytes() {
  const view = buildProtocolPayloadView({
    direction: "SEND",
    message: "MSG_AS_REP",
    payloadHex: "aa",
    payloadPlainHex: "010203040506070812000000000000000100000000000000020002aabb",
    payloadEncryptedHex: "bb",
  });

  assertCondition(rowValue(view.rows, "kc_tgs") === "0x0102030405060708", "AS_REP kc_tgs is not shown as bytes");
  assertCondition(rowValue(view.rows, "ticket_tgs") === "encrypted field", "AS_REP ticket field missing");
}

function testEncryptedSignedAppWithoutPlainIsNotParsedAsSignedApp() {
  const fakeCipherThatLooksLikeMove = "09000201020000";
  const view = buildProtocolPayloadView({
    direction: "RECV",
    message: "MSG_APP.GAME_MOVE",
    payloadHex: fakeCipherThatLooksLikeMove,
    payloadEncryptedHex: fakeCipherThatLooksLikeMove,
  });

  assertCondition(rowValue(view.rows, "payload_state") === "encrypted", "encrypted MSG_APP parsed as signed app");
  assertCondition(view.hexBlocks[0]?.title === "payload encrypted hex", "encrypted MSG_APP hex mislabeled");
}

function testPlainGameAppWithoutPayloadViewIsParsedAsRawGameMessage() {
  const view = buildProtocolPayloadView({
    direction: "SEND",
    message: "MSG_APP.GAME_MOVE",
    payloadHex: "020100",
  });

  assertCondition(rowValue(view.rows, "game_msg") === "MOVE x=1 y=0", "plain raw GAME_MOVE was not parsed");
  assertCondition(view.hexBlocks[0]?.title === "payload hex", "plain raw GAME_MOVE hex mislabeled");
}

function testVAuthRepAndCertV2CPlainPayloadsAreParsed() {
  const vAuthView = buildProtocolPayloadView({
    direction: "SEND",
    message: "MSG_V_AUTH_REP",
    payloadHex: "aa",
    payloadPlainHex: "0000000000001234",
    payloadEncryptedHex: "bb",
  });
  assertCondition(rowValue(vAuthView.rows, "ts5_plus_1") === "0x0000000000001234", "V_AUTH_REP not parsed");

  const certView = buildProtocolPayloadView({
    direction: "SEND",
    message: "MSG_CERT_V2C",
    payloadHex: "aa",
    payloadPlainHex: "130003aabbcc",
    payloadEncryptedHex: "bb",
  });
  assertCondition(rowValue(certView.rows, "v_id") === "V", "CERT_V2C v_id not parsed");
  assertCondition(rowValue(certView.rows, "cert") === "aa bb cc", "CERT_V2C cert not summarized");
}

testEncryptedOuterPayloadIsNotParsedAsPlain();
testPlainAsRepShowsActualSessionKeyBytes();
testEncryptedSignedAppWithoutPlainIsNotParsedAsSignedApp();
testPlainGameAppWithoutPayloadViewIsParsedAsRawGameMessage();
testVAuthRepAndCertV2CPlainPayloadsAreParsed();

console.log("protocolPayloadSelftest: ok");
