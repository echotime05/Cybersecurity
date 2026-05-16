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
  private readonly events: ProtocolEvent[] = [];
  private readonly maxEvents = 500;

  private selectedId?: number;
  private activeRole = "All";
  private connected = false;

  private readonly gameTab = document.getElementById("tab-game") as HTMLButtonElement;
  private readonly protocolTab = document.getElementById("tab-protocol") as HTMLButtonElement;
  private readonly panel = document.getElementById("protocol-panel")!;
  private readonly filtersEl = document.getElementById("protocol-filters")!;
  private readonly eventsEl = document.getElementById("protocol-events")!;
  private readonly titleEl = document.getElementById("protocol-title")!;
  private readonly metaEl = document.getElementById("protocol-meta")!;
  private readonly selectedEl = document.getElementById("protocol-selected")!;
  private readonly headerGridEl = document.getElementById("protocol-header-grid")!;
  private readonly payloadEl = document.getElementById("protocol-payload")!;

  constructor(monitorUrl: string) {
    this.network = new ProtocolMonitorNetwork(monitorUrl);
    this.network.onEvent = (event) => this.addEvent(event);
    this.network.onStatus = (status) => {
      this.connected = status === "connected";
      this.renderDetail();
    };
    this.gameTab.addEventListener("click", () => this.setVisible(false));
    this.protocolTab.addEventListener("click", () => this.setVisible(true));
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
    this.events.push(event);
    if (this.events.length > this.maxEvents) {
      this.events.shift();
    }
    this.selectedId = event.id;
    this.render();
  }

  private filteredEvents() {
    if (this.activeRole === "All") {
      return this.events;
    }
    return this.events.filter((event) => event.role === this.activeRole);
  }

  private render() {
    this.renderFilters();
    this.renderEvents();
    this.renderDetail();
  }

  private renderFilters() {
    const roles = ["All", ...Array.from(new Set(this.events.map((event) => event.role))).sort()];
    if (!roles.includes(this.activeRole)) {
      this.activeRole = "All";
    }

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
        button.addEventListener("click", () => {
          this.selectedId = event.id;
          this.render();
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
  }

  private renderDetail() {
    const selected =
      this.events.find((event) => event.id === this.selectedId) ?? this.latestFilteredEvent();
    if (!selected) {
      this.titleEl.textContent = this.connected ? "" : "Monitor disconnected";
      this.metaEl.textContent = "";
      this.selectedEl.textContent = "";
      this.headerGridEl.replaceChildren();
      this.payloadEl.textContent = "";
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
    this.payloadEl.textContent = this.formatPayload(selected.payloadHex);
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

  private formatPayload(payloadHex: string) {
    return payloadHex.match(/.{1,2}/g)?.join(" ") ?? "";
  }

  private categoryClass(category: ProtocolCategory) {
    return `protocol-category-${category}`;
  }

  private latestFilteredEvent() {
    const filtered = this.filteredEvents();
    return filtered.length > 0 ? filtered[filtered.length - 1] : undefined;
  }
}
