export type TeamState = { teamId: number; score: number; tanks: number };

export type TankState = {
  clientId: number;
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

export type LoginStatus = "idle" | "authenticating" | "authenticated" | "failed";
export type JoinStatus = "joined";

export type LoginState = {
  type: "loginState";
  status: LoginStatus;
  clientId?: number;
  vServer?: string;
  message?: string;
};

export type JoinState = {
  type: "joinState";
  status: JoinStatus;
};

export class Network {
  socket!: WebSocket;
  self = 0;
  state?: BattleState;
  onState?: (state: BattleState) => void;
  onLoginState?: (state: LoginState) => void;
  onJoinState?: (state: JoinState) => void;

  constructor(private serverUrl: string) {}

  async connect(): Promise<Network> {
    this.socket = new WebSocket(this.serverUrl);
    this.socket.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.type === "state") {
        this.self = message.self;
        this.state = message.state;
        this.onState?.(message.state);
      } else if (message.type === "loginState") {
        this.onLoginState?.(message);
      } else if (message.type === "joinState") {
        this.onJoinState?.(message);
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

  sendLogin(clientId: number, password: string) {
    this.send({ type: "login", clientId, password });
  }

  sendJoin() {
    this.send({ type: "join" });
  }

  sendTarget(angle: number) {
    this.send({ type: "target", angle });
  }

  sendShoot() {
    this.send({ type: "shoot" });
  }

}
