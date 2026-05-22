import { ProtocolEventStore } from "../src/protocolEventStore";

type TestEvent = {
  id: number;
  role: string;
  message: string;
};

function assertCondition(condition: boolean, message: string) {
  if (!condition) {
    throw new Error(message);
  }
}

function event(id: number, message = "MSG_APP.GAME_STATE"): TestEvent {
  return { id, role: "Client3", message };
}

function eventIds(events: TestEvent[]) {
  return events.map((item) => item.id).join(",");
}

function testPauseFreezesVisibleBuffers() {
  const store = new ProtocolEventStore<TestEvent>({
    maxEventsPerRole: 2,
    maxNoisyEventsPerRole: 2,
    isNoisy: (item) => item.message === "MSG_APP.GAME_STATE",
  });

  store.addEvent(event(1), false);
  store.addEvent(event(2), false);
  assertCondition(eventIds(store.filteredEvents("Client3", true)) === "1,2", "initial noisy buffer is wrong");

  store.addEvent(event(3), true);
  store.addEvent(event(4), true);

  const frozen = store.filteredEvents("Client3", true);
  assertCondition(eventIds(frozen) === "1,2", "paused monitor should not trim visible noisy rows");
  assertCondition(
    store.findVisibleEvent("Client3", true, 1)?.id === 1,
    "paused monitor should still find a visible selected event"
  );
}

function testUnpausedNoisyBufferStillTrims() {
  const store = new ProtocolEventStore<TestEvent>({
    maxEventsPerRole: 2,
    maxNoisyEventsPerRole: 2,
    isNoisy: (item) => item.message === "MSG_APP.GAME_STATE",
  });

  store.addEvent(event(1), false);
  store.addEvent(event(2), false);
  store.addEvent(event(3), false);

  assertCondition(eventIds(store.filteredEvents("Client3", true)) === "2,3", "unpaused noisy buffer should keep newest rows");
}

testPauseFreezesVisibleBuffers();
testUnpausedNoisyBufferStillTrims();

console.log("protocolEventStoreSelftest: ok");
