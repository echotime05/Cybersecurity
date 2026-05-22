import {
  buildProtocolPayloadView,
  formatHexString,
  type ProtocolPayloadFieldCard,
  type ProtocolPayloadRow,
} from "./protocolPayload";
import { ProtocolEventStore } from "./protocolEventStore";

type ProtocolCategory = "app" | "kerberos" | "error";

type ProtocolField = {
  label: string;
  raw: string;
};

type ProtocolHeader = {
  msgType: ProtocolField;
  src: ProtocolField;
  dst: ProtocolField;
  payloadLen: ProtocolField;
  reserved: ProtocolField;
};

export type ProtocolEvent = {
  type: "protocolEvent";
  id: number;
  timestamp: string;
  role: string;
  direction: "SEND" | "RECV";
  endpoint: string;
  message: string;
  category: ProtocolCategory;
  header: ProtocolHeader;
  payloadHex: string;
  payloadPlainHex?: string;
  payloadEncryptedHex?: string;
  payloadFields?: Array<{
    name: string;
    plainHex?: string;
    encryptedHex?: string;
  }>;
};

class ProtocolMonitorNetwork {
  socket?: WebSocket;
  onEvent?: (event: ProtocolEvent) => void;
  onStatus?: (status: "connected" | "disconnected") => void;

  constructor(private readonly url: string) {}

  connect() {
    this.socket = new WebSocket(this.url);
    this.socket.onopen = () => this.onStatus?.("connected");
    this.socket.onclose = () => this.onStatus?.("disconnected");
    this.socket.onerror = () => this.onStatus?.("disconnected");
    this.socket.onmessage = (message) => {
      const parsed = JSON.parse(message.data);
      if (parsed.type === "protocolEvent") {
        this.onEvent?.(parsed);
      }
    };
  }
}

export class ProtocolMonitorUi {
  private readonly network: ProtocolMonitorNetwork;
  private readonly eventStore: ProtocolEventStore<ProtocolEvent>;

  private selectedId?: number;
  private activeRole = "All";
  private connected = false;
  private paused = false;
  private showNoisy = false;

  private readonly gameTab = document.getElementById("tab-game") as HTMLButtonElement;
  private readonly protocolTab = document.getElementById("tab-protocol") as HTMLButtonElement;
  private readonly panel = document.getElementById("protocol-panel")!;
  private readonly pauseButton = document.getElementById("protocol-pause") as HTMLButtonElement;
  private readonly noisyButton = document.getElementById("protocol-noisy") as HTMLButtonElement;
  private readonly filtersEl = document.getElementById("protocol-filters")!;
  private readonly eventsEl = document.getElementById("protocol-events")!;
  private readonly titleEl = document.getElementById("protocol-title")!;
  private readonly metaEl = document.getElementById("protocol-meta")!;
  private readonly selectedEl = document.getElementById("protocol-selected")!;
  private readonly headerGridEl = document.getElementById("protocol-header-grid")!;
  private readonly payloadEl = document.getElementById("protocol-payload")!;
  private readonly payloadHexEl = document.getElementById("protocol-payload-hex")!;

  constructor(monitorUrl: string) {
    this.network = new ProtocolMonitorNetwork(monitorUrl);
    this.eventStore = new ProtocolEventStore<ProtocolEvent>({
      maxEventsPerRole: 500,
      maxNoisyEventsPerRole: 120,
      isNoisy: (event) => this.isNoisy(event),
    });
    this.network.onEvent = (event) => this.addEvent(event);
    this.network.onStatus = (status) => {
      this.connected = status === "connected";
      this.renderDetail();
    };
    this.gameTab.addEventListener("click", () => this.setVisible(false));
    this.protocolTab.addEventListener("click", () => this.setVisible(true));
    this.pauseButton.addEventListener("click", () => this.togglePause());
    this.noisyButton.addEventListener("click", () => this.toggleNoisy());
    this.render();
  }

  connect() {
    try {
      this.network.connect();
    } catch {
      this.connected = false;
      this.renderDetail();
    }
  }

  private setVisible(visible: boolean) {
    this.panel.classList.toggle("hidden", !visible);
    this.gameTab.classList.toggle("active", !visible);
    this.protocolTab.classList.toggle("active", visible);
  }

  private addEvent(event: ProtocolEvent) {
    if (!this.eventStore.addEvent(event, this.paused)) {
      return;
    }

    const eventIsVisible = this.eventMatchesCurrentView(event);
    if (!this.paused && eventIsVisible) {
      this.selectedId = event.id;
    }
    if (!eventIsVisible && this.selectedId !== undefined) {
      return;
    }
    this.render();
  }

  private filteredEvents() {
    return this.eventStore.filteredEvents(this.activeRole, this.showNoisy);
  }

  private render() {
    this.renderFilters();
    this.renderEvents();
    this.renderDetail();
  }

  private renderFilters() {
    const roles = ["All", ...this.eventStore.roles()];
    if (!roles.includes(this.activeRole)) {
      this.activeRole = "All";
    }
    this.pauseButton.textContent = this.paused ? "Resume" : "Pause";
    this.pauseButton.classList.toggle("active", this.paused);
    this.noisyButton.classList.toggle("active", this.showNoisy);

    this.filtersEl.replaceChildren(
      ...roles.map((role) => {
        const button = document.createElement("button");
        button.className = `protocol-filter${role === this.activeRole ? " active" : ""}`;
        button.type = "button";
        button.textContent = role;
        button.addEventListener("click", () => {
          this.activeRole = role;
          const filtered = this.filteredEvents();
          const latest = filtered.length > 0 ? filtered[filtered.length - 1] : undefined;
          this.selectedId = latest?.id;
          this.render();
        });
        return button;
      })
    );
  }

  private renderEvents() {
    const scrollTop = this.eventsEl.scrollTop;
    const items = this.filteredEvents().slice().reverse();
    this.eventsEl.replaceChildren(
      ...items.map((event) => {
        const button = document.createElement("button");
        button.className = [
          "protocol-event-item",
          event.id === this.selectedId ? "active" : "",
          this.categoryClass(event.category),
        ]
          .filter(Boolean)
          .join(" ");
        button.type = "button";
        button.dataset.eventId = String(event.id);
        button.addEventListener("click", () => {
          this.selectedId = event.id;
          this.updateEventSelection();
          this.renderDetail();
        });

        const time = document.createElement("span");
        time.className = "line";
        time.textContent = `${event.timestamp} . ${event.direction}`;
        const endpoint = document.createElement("span");
        endpoint.className = "line";
        endpoint.textContent = this.formatEndpoint(event.endpoint);
        const message = document.createElement("span");
        message.className = "line message";
        message.textContent = event.message;

        button.append(time, endpoint, message);
        return button;
      })
    );
    this.eventsEl.scrollTop = scrollTop;
  }

  private updateEventSelection() {
    this.eventsEl.querySelectorAll<HTMLElement>(".protocol-event-item").forEach((item) => {
      item.classList.toggle("active", item.dataset.eventId === String(this.selectedId));
    });
  }

  private renderDetail() {
    const selected =
      this.findVisibleEvent(this.selectedId) ?? this.latestFilteredEvent();
    if (!selected) {
      this.titleEl.textContent = this.connected ? "" : "Monitor disconnected";
      this.metaEl.textContent = "";
      this.selectedEl.textContent = "";
      this.headerGridEl.replaceChildren();
      this.payloadEl.replaceChildren();
      this.payloadHexEl.replaceChildren();
      return;
    }

    this.titleEl.textContent = this.formatEndpoint(selected.endpoint);
    this.titleEl.className = this.categoryClass(selected.category);
    this.metaEl.textContent = `${selected.timestamp} . ${selected.direction} . ${selected.message}`;
    this.metaEl.className = this.categoryClass(selected.category);
    this.selectedEl.textContent = `selected packet #${String(selected.id).padStart(3, "0")}`;
    this.headerGridEl.replaceChildren(
      this.headerCell("MSG_TYPE", selected.header.msgType),
      this.headerCell("SRC", selected.header.src),
      this.headerCell("DST", selected.header.dst),
      this.headerCell("PAYLOAD_LEN", selected.header.payloadLen),
      this.headerCell("RESERVED", selected.header.reserved)
    );
    this.renderPayload(selected);
  }

  private headerCell(name: string, field: ProtocolField) {
    const cell = document.createElement("div");
    cell.className = "protocol-header-cell";

    const nameEl = document.createElement("div");
    nameEl.className = "field-name";
    nameEl.textContent = name;
    const labelEl = document.createElement("div");
    labelEl.className = "field-label";
    labelEl.textContent = field.label;
    const rawEl = document.createElement("div");
    rawEl.className = "field-raw";
    rawEl.textContent = field.raw;

    cell.append(nameEl, labelEl, rawEl);
    return cell;
  }

  private formatEndpoint(endpoint: string) {
    return endpoint.replace("->", " -> ");
  }

  private categoryClass(category: ProtocolCategory) {
    return `protocol-category-${category}`;
  }

  private renderPayload(event: ProtocolEvent) {
    const view = buildProtocolPayloadView(event);
    const rows = document.createElement("div");
    rows.className = "protocol-payload-rows";
    rows.append(...view.rows.map((row) => this.payloadRow(row)));

    const layout = document.createElement("div");
    layout.className = view.fieldCards.length > 0
      ? "protocol-payload-layout with-nested"
      : "protocol-payload-layout single";
    layout.append(rows);

    if (view.fieldCards.length > 0) {
      const nested = document.createElement("div");
      nested.className = "protocol-payload-nested";
      nested.append(...view.fieldCards.map((card) => this.payloadFieldCard(card)));
      layout.append(nested);
    }

    this.payloadEl.replaceChildren(layout);

    this.payloadHexEl.className =
      view.hexBlocks.length === 1 ? "protocol-hex-grid single" : "protocol-hex-grid";
    this.payloadHexEl.replaceChildren(
      ...view.hexBlocks.map((block) => {
        const box = document.createElement("div");
        box.className = "protocol-hex-box";

        const title = document.createElement("div");
        title.className = `protocol-hex-head protocol-tone-${block.tone}`;
        title.textContent = block.title;

        const body = document.createElement("div");
        body.className = `protocol-hex-body protocol-tone-${block.tone}`;
        body.textContent = formatHexString(block.hex);

        box.append(title, body);
        return box;
      })
    );
  }

  private payloadRow(row: ProtocolPayloadRow) {
    const key = document.createElement("div");
    key.className = "protocol-payload-key";
    key.textContent = row.key;

    const value = document.createElement("div");
    value.className = `protocol-payload-value${row.tone ? ` protocol-tone-${row.tone}` : ""}`;
    value.textContent = row.value;

    const fragment = document.createDocumentFragment();
    fragment.append(key, value);
    return fragment;
  }

  private payloadFieldCard(card: ProtocolPayloadFieldCard) {
    const box = document.createElement("div");
    box.className = "protocol-payload-card";

    const title = document.createElement("div");
    title.className = "protocol-payload-card-title";
    title.textContent = card.title;

    const rows = document.createElement("div");
    rows.className = "protocol-payload-card-rows";
    rows.append(...card.rows.map((row) => this.payloadRow(row)));

    box.append(title, rows);
    return box;
  }

  private togglePause() {
    this.paused = !this.paused;
    this.render();
  }

  private toggleNoisy() {
    this.showNoisy = !this.showNoisy;
    this.selectedId = this.latestFilteredEvent()?.id;
    this.render();
  }

  private eventMatchesCurrentView(event: ProtocolEvent) {
    if (this.activeRole !== "All" && event.role !== this.activeRole) {
      return false;
    }
    return this.showNoisy || !this.isNoisy(event);
  }

  private isNoisy(event: ProtocolEvent) {
    return (
      event.message === "MSG_APP.GAME_STATE" ||
      event.message === "MSG_APP.APP_ACK" ||
      event.message === "MSG_APP.GAME_TARGET"
    );
  }

  private findVisibleEvent(id?: number) {
    return this.eventStore.findVisibleEvent(this.activeRole, this.showNoisy, id);
  }

  private latestFilteredEvent() {
    return this.eventStore.latestFilteredEvent(this.activeRole, this.showNoisy);
  }
}
