export type TeamState = { teamId: number; score: number; tanks: number };

export type TankState = {
  clientId: number;
  name: string;
  team: number;
  x: number;
  y: number;
  angle: number;
  hp: number;
  shield: number;
  dead: boolean;
  score: number;
};

export type BulletState = {
  id: number;
  ownerClientId: number;
  x: number;
  y: number;
  special: boolean;
};

export type PickableState = { id: number; type: number; x: number; y: number };

export type BattleState = {
  serverTimeMs: number;
  totalScore: number;
  winnerTeam: number;
  teams: TeamState[];
  tanks: TankState[];
  bullets: BulletState[];
  pickables: PickableState[];
};

export class Network {
  socket!: WebSocket;
  self = 0;
  state?: BattleState;
  onState?: (state: BattleState) => void;

  constructor(private serverUrl: string) {}

  async connect(): Promise<Network> {
    this.socket = new WebSocket(this.serverUrl);
    this.socket.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.type === "state") {
        this.self = message.self;
        this.state = message.state;
        this.onState?.(message.state);
      }
    };
    await new Promise<void>((resolve, reject) => {
      this.socket.onopen = () => resolve();
      this.socket.onerror = () => reject(new Error("WebSocket connection failed"));
    });
    return this;
  }

  private send(value: unknown) {
    if (this.socket?.readyState === WebSocket.OPEN) {
      this.socket.send(JSON.stringify(value));
    }
  }

  sendMove(x: number, y: number) {
    this.send({ type: "move", x, y });
  }

  sendTarget(angle: number) {
    this.send({ type: "target", angle });
  }

  sendShoot(shooting: boolean) {
    this.send({ type: "shoot", shooting });
  }

  sendName(name: string) {
    this.send({ type: "name", name });
  }
}
