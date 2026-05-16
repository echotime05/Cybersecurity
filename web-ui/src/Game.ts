import * as THREE from "three";
import {
  BattleState,
  BulletState,
  Network,
  PickableState,
  TankState,
  TeamState,
} from "./Network";
import { TankEntity, preloadTankModel } from "./Tank";
import { MapRenderer } from "./MapRenderer";
import { ProtocolMonitorUi } from "./ProtocolMonitor";
import { Sound } from "./Sound";

const TEAM_COLORS = [0xff4444, 0x4488ff, 0x44ff44, 0xffff44];
const TEAM_NAMES = ["Red", "Blue", "Green", "Yellow"];

function serverAngleToVisual(angle: number) {
  return ((90 - angle) % 360 + 360) % 360;
}

function pickableColor(type: number) {
  if (type === 1) return 0x44ff44;
  if (type === 2) return 0xff4444;
  if (type === 3) return 0x4488ff;
  return 0xffffff;
}

function disposeObject(root: THREE.Object3D) {
  root.traverse((child) => {
    const mesh = child as THREE.Mesh;
    if (mesh.geometry) {
      mesh.geometry.dispose();
    }
    const material = mesh.material as THREE.Material | THREE.Material[] | undefined;
    if (Array.isArray(material)) {
      for (const item of material) item.dispose();
    } else if (material) {
      material.dispose();
    }
  });
}

export class Game {
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;

  network: Network;
  protocolMonitor: ProtocolMonitorUi;
  sound: Sound;
  map!: MapRenderer;

  tanks = new Map<string, TankEntity>();
  bulletMeshes = new Map<string, THREE.Mesh>();
  pickableMeshes = new Map<string, THREE.Group>();

  mySessionId = "";
  connected = false;
  joined = false;
  currentState?: BattleState;
  lastWinnerTeam = -1;

  keys = new Set<string>();
  mouseX = 0;
  mouseY = 0;
  mouseDown = false;

  lastSentDirX = -999;
  lastSentDirY = -999;
  lastSentAngle = -999;
  lastTargetSendTime = 0;

  raycaster = new THREE.Raycaster();
  groundPlane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);

  healthFill!: HTMLElement;
  shieldFill!: HTMLElement;
  scoresList!: HTMLElement;
  scoreElements = new Map<number, HTMLElement>();
  deathScreen!: HTMLElement;
  winnerScreen!: HTMLElement;
  ammoDisplay!: HTMLElement;
  connectStatus!: HTMLElement;
  authOverlay!: HTMLElement;
  loginPanel!: HTMLElement;
  joinPanel!: HTMLElement;
  loginClient!: HTMLSelectElement;
  loginPassword!: HTMLInputElement;
  loginSubmit!: HTMLButtonElement;
  loginMessage!: HTMLElement;
  joinClientId!: HTMLElement;
  joinVServer!: HTMLElement;
  joinSubmit!: HTMLButtonElement;

  constructor() {
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x0d1b2a);
    this.scene.fog = new THREE.Fog(0x0d1b2a, 35, 65);

    const frustumSize = 22;
    const aspect = window.innerWidth / window.innerHeight;
    this.camera = new THREE.OrthographicCamera(
      (-frustumSize * aspect) / 2,
      (frustumSize * aspect) / 2,
      frustumSize / 2,
      -frustumSize / 2,
      0.1,
      200
    );
    this.camera.position.set(44, 20, 44);
    this.camera.lookAt(24, 0, 24);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(window.innerWidth, window.innerHeight);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    document.body.prepend(this.renderer.domElement);

    const ambient = new THREE.AmbientLight(0x8888aa, 0.7);
    this.scene.add(ambient);

    const sun = new THREE.DirectionalLight(0xffeedd, 1.2);
    sun.position.set(30, 40, 20);
    sun.castShadow = true;
    sun.shadow.mapSize.set(2048, 2048);
    sun.shadow.camera.left = -30;
    sun.shadow.camera.right = 30;
    sun.shadow.camera.top = 30;
    sun.shadow.camera.bottom = -30;
    this.scene.add(sun);

    this.map = new MapRenderer(this.scene);
    this.sound = new Sound();

    const params = new URLSearchParams(window.location.search);
    const serverUrl = params.get("client") || "ws://127.0.0.1:7001";
    const monitorUrl = params.get("monitor") || "ws://127.0.0.1:7010";
    this.network = new Network(serverUrl);
    this.protocolMonitor = new ProtocolMonitorUi(monitorUrl);

    this.healthFill = document.getElementById("health-fill")!;
    this.shieldFill = document.getElementById("shield-fill")!;
    this.scoresList = document.getElementById("scores-list")!;
    this.deathScreen = document.getElementById("death-screen")!;
    this.winnerScreen = document.getElementById("winner-screen")!;
    this.ammoDisplay = document.getElementById("ammo-display")!;
    this.connectStatus = document.getElementById("connect-status")!;
    this.authOverlay = document.getElementById("auth-overlay")!;
    this.loginPanel = document.getElementById("login-panel")!;
    this.joinPanel = document.getElementById("join-panel")!;
    this.loginClient = document.getElementById("login-client") as HTMLSelectElement;
    this.loginPassword = document.getElementById("login-password") as HTMLInputElement;
    this.loginSubmit = document.getElementById("login-submit") as HTMLButtonElement;
    this.loginMessage = document.getElementById("login-message")!;
    this.joinClientId = document.getElementById("join-client-id")!;
    this.joinVServer = document.getElementById("join-v-server")!;
    this.joinSubmit = document.getElementById("join-submit") as HTMLButtonElement;

    this.setupInput();
    this.setupAuthUi();
    window.addEventListener("resize", () => this.onResize());
  }

  async start() {
    await preloadTankModel();

    this.network.onState = (state) => {
      this.mySessionId = String(this.network.self);
      this.connected = true;
      this.reconcileState(state);
    };
    this.network.onLoginState = (state) => {
      if (state.status === "authenticating") {
        this.loginSubmit.disabled = true;
        this.loginMessage.textContent = "Authenticating";
      } else if (state.status === "authenticated") {
        this.loginSubmit.disabled = false;
        this.loginPanel.classList.add("hidden");
        this.joinPanel.classList.remove("hidden");
        this.joinSubmit.disabled = false;
        this.joinClientId.textContent = `Client${state.clientId ?? ""}`;
        this.joinVServer.textContent = state.vServer ?? "";
        this.loginPassword.value = "";
      } else if (state.status === "failed") {
        this.loginSubmit.disabled = false;
        this.loginMessage.textContent = state.message ?? "Login failed";
      }
    };
    this.network.onJoinState = (state) => {
      if (state.status === "joined") {
        this.joined = true;
        this.authOverlay.classList.add("hidden");
      }
    };

    try {
      this.protocolMonitor.connect();
      await this.network.connect();
      this.connectStatus.style.display = "none";
    } catch (e) {
      this.connectStatus.textContent = "Failed to connect. Is client bridge running?";
      console.error(e);
      return;
    }

    this.animate();
  }

  private reconcileState(state: BattleState) {
    this.currentState = state;
    const seenTanks = new Set<string>();

    for (const tank of state.tanks) {
      const key = String(tank.clientId);
      seenTanks.add(key);
      let entity = this.tanks.get(key);
      if (!entity) {
        entity = new TankEntity(tank.team);
        entity.targetX = tank.x;
        entity.targetZ = tank.y;
        entity.group.position.set(tank.x, 0, tank.y);
        this.scene.add(entity.group);
        this.tanks.set(key, entity);
      }

      const previousHp = entity.dead ? tank.hp : this.getTankHpWidth(entity);
      const wasDead = entity.dead;
      entity.targetX = tank.x;
      entity.targetZ = tank.y;
      entity.targetAngle = serverAngleToVisual(tank.angle);
      entity.setHealth(tank.hp);
      entity.setShield(tank.shield);
      entity.setDead(tank.dead);

      if (tank.dead && !wasDead) {
        this.sound.explosion();
      }
      if (key === this.mySessionId) {
        this.healthFill.style.width = `${Math.max(0, tank.hp) * 10}%`;
        this.shieldFill.style.width = `${Math.max(0, tank.shield) * 10}%`;
        this.deathScreen.style.display = tank.dead ? "block" : "none";
      } else if (tank.hp < previousHp) {
        const myTank = this.tanks.get(this.mySessionId);
        if (myTank) {
          const dx = entity.group.position.x - myTank.group.position.x;
          const dz = entity.group.position.z - myTank.group.position.z;
          const dist = Math.sqrt(dx * dx + dz * dz);
          const vol = Math.max(0, 0.25 * (1 - dist / 25));
          if (vol > 0.01) this.sound.hit(vol);
        }
      }
    }

    for (const [key, entity] of this.tanks) {
      if (!seenTanks.has(key)) {
        this.scene.remove(entity.group);
        entity.dispose();
        this.tanks.delete(key);
      }
    }

    this.reconcileBullets(state.bullets, state.tanks);
    this.reconcilePickables(state.pickables);
    this.updateScoresFromSnapshot(state.teams);
    this.ammoDisplay.textContent = "";

    if (state.winnerTeam >= 0 && state.winnerTeam !== this.lastWinnerTeam) {
      this.lastWinnerTeam = state.winnerTeam;
      this.showWinnerScreen(state.winnerTeam, state);
    } else if (state.winnerTeam < 0) {
      this.lastWinnerTeam = -1;
    }
  }

  private getTankHpWidth(entity: TankEntity) {
    return Math.round(entity.healthBar.scale.x / 1.4 * 10);
  }

  private reconcileBullets(bullets: BulletState[], tanks: TankState[]) {
    const tankById = new Map(tanks.map((tank) => [tank.clientId, tank]));
    const seenBullets = new Set<string>();
    for (const bullet of bullets) {
      const key = String(bullet.id);
      seenBullets.add(key);
      let mesh = this.bulletMeshes.get(key);
      if (!mesh) {
        const owner = tankById.get(bullet.ownerClientId);
        const bulletColor = bullet.special
          ? 0xff8800
          : TEAM_COLORS[owner?.team ?? 0] || 0xffff66;
        const geo = new THREE.SphereGeometry(bullet.special ? 0.2 : 0.12, 6, 6);
        const mat = new THREE.MeshBasicMaterial({ color: bulletColor });
        mesh = new THREE.Mesh(geo, mat);
        this.scene.add(mesh);
        this.bulletMeshes.set(key, mesh);
        if (String(bullet.ownerClientId) === this.mySessionId) {
          bullet.special ? this.sound.shootSpecial() : this.sound.shoot();
        }
      }
      (mesh as any)._sx = bullet.x;
      (mesh as any)._sy = bullet.y;
      mesh.position.set(bullet.x, 1.5, bullet.y);
    }

    for (const [key, mesh] of this.bulletMeshes) {
      if (!seenBullets.has(key)) {
        this.scene.remove(mesh);
        mesh.geometry.dispose();
        const material = mesh.material as THREE.Material;
        material.dispose();
        this.bulletMeshes.delete(key);
      }
    }
  }

  private reconcilePickables(pickables: PickableState[]) {
    const seenPickables = new Set<string>();
    for (const pickable of pickables) {
      const key = String(pickable.id);
      seenPickables.add(key);
      let group = this.pickableMeshes.get(key);
      if (!group) {
        group = this.createPickable(pickable);
        this.scene.add(group);
        this.pickableMeshes.set(key, group);
      }
      group.position.set(pickable.x, 0.6, pickable.y);
    }

    for (const [key, group] of this.pickableMeshes) {
      if (!seenPickables.has(key)) {
        this.scene.remove(group);
        disposeObject(group);
        this.pickableMeshes.delete(key);
      }
    }
  }

  private createPickable(pickable: PickableState) {
    const group = new THREE.Group();
    const color = pickableColor(pickable.type);
    const mat = new THREE.MeshStandardMaterial({
      color,
      emissive: color,
      emissiveIntensity: 0.4,
    });

    if (pickable.type === 1) {
      group.add(new THREE.Mesh(new THREE.BoxGeometry(0.7, 0.2, 0.15), mat));
      group.add(new THREE.Mesh(new THREE.BoxGeometry(0.2, 0.7, 0.15), mat));
    } else if (pickable.type === 3) {
      const shape = new THREE.Shape();
      shape.moveTo(0, 0.4);
      shape.lineTo(0.35, 0.25);
      shape.lineTo(0.35, 0);
      shape.quadraticCurveTo(0.3, -0.3, 0, -0.45);
      shape.quadraticCurveTo(-0.3, -0.3, -0.35, 0);
      shape.lineTo(-0.35, 0.25);
      shape.closePath();
      const geo = new THREE.ExtrudeGeometry(shape, { depth: 0.12, bevelEnabled: false });
      geo.center();
      group.add(new THREE.Mesh(geo, mat));
    } else {
      group.add(new THREE.Mesh(new THREE.OctahedronGeometry(0.35), mat));
    }

    const glowGeo = new THREE.SphereGeometry(0.5, 8, 8);
    const glowMat = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.15,
    });
    group.add(new THREE.Mesh(glowGeo, glowMat));
    group.position.set(pickable.x, 0.6, pickable.y);
    return group;
  }

  private updateScoresFromSnapshot(teams: TeamState[]) {
    const ranked = [...teams].sort((a, b) => b.score - a.score);
    const rowHeight = 28;

    for (const team of ranked) {
      if (!this.scoreElements.has(team.teamId)) {
        const el = document.createElement("div");
        el.className = `team-score team-${team.teamId}`;
        el.innerHTML = `<span class="team-name">${TEAM_NAMES[team.teamId]}</span><span class="team-pts">0</span>`;
        this.scoresList.appendChild(el);
        this.scoreElements.set(team.teamId, el);
      }
    }

    for (let rank = 0; rank < ranked.length; rank++) {
      const team = ranked[rank];
      const el = this.scoreElements.get(team.teamId)!;
      el.style.top = `${rank * rowHeight}px`;
      el.querySelector(".team-pts")!.textContent = `${team.score}`;
    }

    this.scoresList.style.height = `${ranked.length * rowHeight}px`;
  }

  private showWinnerScreen(winnerTeamId: number, state: BattleState) {
    const stripeRgba = [
      "rgba(200,40,40,0.92)",
      "rgba(40,100,220,0.92)",
      "rgba(40,180,40,0.92)",
      "rgba(200,200,40,0.92)",
    ];
    const lineRgba = [
      "rgba(255,120,120,0.8)",
      "rgba(100,170,255,0.8)",
      "rgba(100,255,100,0.8)",
      "rgba(255,255,100,0.8)",
    ];
    const tintRgba = [
      "rgba(255,0,0,0.08)",
      "rgba(0,80,255,0.08)",
      "rgba(0,200,0,0.08)",
      "rgba(200,200,0,0.08)",
    ];

    const myTank = state.tanks.find((tank) => String(tank.clientId) === this.mySessionId);
    const isWinner = myTank && myTank.team === winnerTeamId;

    const label = document.getElementById("winner-label")!;
    const teamLine = document.getElementById("winner-team")!;
    const stripe = document.getElementById("winner-stripe")!;
    const lineTop = document.getElementById("winner-line-top")!;
    const lineBot = document.getElementById("winner-line-bot")!;
    const tint = document.getElementById("winner-tint")!;

    label.textContent = isWinner ? "VICTORY" : "DEFEAT";
    label.style.color = "#fff";
    teamLine.textContent = `${TEAM_NAMES[winnerTeamId]} Team Wins`;
    teamLine.style.color = "rgba(255,255,255,0.9)";

    stripe.style.background = stripeRgba[winnerTeamId] || stripeRgba[0];
    lineTop.style.background = lineRgba[winnerTeamId] || lineRgba[0];
    lineBot.style.background = lineRgba[winnerTeamId] || lineRgba[0];
    tint.style.background = tintRgba[winnerTeamId] || tintRgba[0];

    this.winnerScreen.className = "ready";
    void this.winnerScreen.offsetWidth;
    this.winnerScreen.className = "ready active";

    setTimeout(() => {
      this.winnerScreen.className = "exit";
      setTimeout(() => {
        this.winnerScreen.className = "";
      }, 800);
    }, 2800);
  }

  private setupInput() {
    window.addEventListener("keydown", (e) => {
      this.keys.add(e.key.toLowerCase());
    });
    window.addEventListener("keyup", (e) => {
      this.keys.delete(e.key.toLowerCase());
    });
    window.addEventListener("mousemove", (e) => {
      this.mouseX = e.clientX;
      this.mouseY = e.clientY;
    });
    window.addEventListener("mousedown", (e) => {
      if (e.button === 0 && this.joined) {
        this.mouseDown = true;
        this.network.sendShoot(true);
      }
    });
    window.addEventListener("mouseup", (e) => {
      if (e.button === 0 && this.joined) {
        this.mouseDown = false;
        this.network.sendShoot(false);
      }
    });
    window.addEventListener("contextmenu", (e) => e.preventDefault());
  }

  private setupAuthUi() {
    this.loginPanel.addEventListener("submit", (event) => {
      event.preventDefault();
      const clientId = Number(this.loginClient.value);
      this.loginSubmit.disabled = true;
      this.loginMessage.textContent = "Authenticating";
      this.network.sendLogin(clientId, this.loginPassword.value);
    });
    this.joinSubmit.addEventListener("click", () => {
      this.joinSubmit.disabled = true;
      this.network.sendJoin();
    });
  }

  private sendInput() {
    if (!this.connected || !this.joined) return;

    let rawX = 0;
    let rawY = 0;
    if (this.keys.has("w") || this.keys.has("arrowup")) rawY -= 1;
    if (this.keys.has("s") || this.keys.has("arrowdown")) rawY += 1;
    if (this.keys.has("a") || this.keys.has("arrowleft")) rawX -= 1;
    if (this.keys.has("d") || this.keys.has("arrowright")) rawX += 1;

    const cameraAngle = -Math.PI / 4;
    const cos = Math.cos(cameraAngle);
    const sin = Math.sin(cameraAngle);
    const dirX = Math.round(rawX * cos - rawY * sin);
    const dirY = Math.round(rawX * sin + rawY * cos);

    if (dirX !== this.lastSentDirX || dirY !== this.lastSentDirY) {
      this.network.sendMove(dirX, dirY);
      this.lastSentDirX = dirX;
      this.lastSentDirY = dirY;
    }

    const myTank = this.tanks.get(this.mySessionId);
    if (!myTank) return;

    const mouse = new THREE.Vector2(
      (this.mouseX / window.innerWidth) * 2 - 1,
      -(this.mouseY / window.innerHeight) * 2 + 1
    );
    this.raycaster.setFromCamera(mouse, this.camera);
    const target = new THREE.Vector3();
    this.raycaster.ray.intersectPlane(this.groundPlane, target);

    const dx = target.x - myTank.group.position.x;
    const dz = target.z - myTank.group.position.z;
    let angle = Math.atan2(dz, dx) * (180 / Math.PI);
    angle = ((angle % 360) + 360) % 360;

    myTank.targetAngle = serverAngleToVisual(angle);

    const now = performance.now();
    if (Math.abs(angle - this.lastSentAngle) > 1 && now - this.lastTargetSendTime >= 33) {
      this.network.sendTarget(angle);
      this.lastSentAngle = angle;
      this.lastTargetSendTime = now;
    }
  }

  private animate = () => {
    requestAnimationFrame(this.animate);

    this.sendInput();

    for (const [, tank] of this.tanks) {
      tank.update(0.016);
    }

    const t = Date.now() * 0.001;
    for (const [, group] of this.pickableMeshes) {
      group.position.y = 0.6 + Math.sin(t * 2) * 0.15;
      group.rotation.y = t;
    }

    for (const [, mesh] of this.bulletMeshes) {
      const data = mesh as any;
      if (data._sx !== undefined) {
        mesh.position.x = THREE.MathUtils.lerp(mesh.position.x, data._sx, 0.4);
        mesh.position.z = THREE.MathUtils.lerp(mesh.position.z, data._sy, 0.4);
      }
    }

    const myTank = this.tanks.get(this.mySessionId);
    if (myTank) {
      const tx = myTank.group.position.x;
      const tz = myTank.group.position.z;
      const nx = (this.mouseX / window.innerWidth) * 2 - 1;
      const ny = (this.mouseY / window.innerHeight) * 2 - 1;
      const lookAhead = 3;
      const offsetX = (nx + ny) * 0.707 * lookAhead;
      const offsetZ = (-nx + ny) * 0.707 * lookAhead;

      this.camera.position.x = THREE.MathUtils.lerp(this.camera.position.x, tx + 20 + offsetX, 0.08);
      this.camera.position.z = THREE.MathUtils.lerp(this.camera.position.z, tz + 20 + offsetZ, 0.08);
      this.camera.position.y = 20;
      this.camera.lookAt(this.camera.position.x - 20, 0, this.camera.position.z - 20);
    }

    this.renderer.render(this.scene, this.camera);
  };

  private onResize() {
    const frustumSize = 22;
    const aspect = window.innerWidth / window.innerHeight;
    this.camera.left = (-frustumSize * aspect) / 2;
    this.camera.right = (frustumSize * aspect) / 2;
    this.camera.top = frustumSize / 2;
    this.camera.bottom = -frustumSize / 2;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(window.innerWidth, window.innerHeight);
  }
}
