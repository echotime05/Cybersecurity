export type ProtocolEventStoreOptions<T extends { id: number; role: string }> = {
  maxEventsPerRole: number;
  maxNoisyEventsPerRole: number;
  isNoisy: (event: T) => boolean;
};

export class ProtocolEventStore<T extends { id: number; role: string }> {
  private readonly eventsByRole = new Map<string, T[]>();
  private readonly noisyEventsByRole = new Map<string, T[]>();
  private readonly seenRoles = new Set<string>();

  constructor(private readonly options: ProtocolEventStoreOptions<T>) {}

  addEvent(event: T, paused: boolean) {
    if (paused) {
      return false;
    }

    this.seenRoles.add(event.role);
    const noisy = this.options.isNoisy(event);
    const buffer = noisy ? this.noisyBufferFor(event.role) : this.bufferFor(event.role);
    buffer.push(event);

    const max = noisy ? this.options.maxNoisyEventsPerRole : this.options.maxEventsPerRole;
    if (buffer.length > max) {
      buffer.shift();
    }
    return true;
  }

  roles() {
    return Array.from(this.seenRoles).sort();
  }

  filteredEvents(activeRole: string, showNoisy: boolean) {
    const roles = activeRole === "All" ? Array.from(this.seenRoles) : [activeRole];
    const events = roles.flatMap((role) => this.eventsForRole(role, showNoisy));
    events.sort((a, b) => a.id - b.id);
    return events;
  }

  findVisibleEvent(activeRole: string, showNoisy: boolean, id?: number) {
    if (id === undefined) {
      return undefined;
    }
    return this.filteredEvents(activeRole, showNoisy).find((event) => event.id === id);
  }

  latestFilteredEvent(activeRole: string, showNoisy: boolean) {
    const filtered = this.filteredEvents(activeRole, showNoisy);
    return filtered.length > 0 ? filtered[filtered.length - 1] : undefined;
  }

  private eventsForRole(role: string, showNoisy: boolean) {
    const normal = this.eventsByRole.get(role) ?? [];
    if (!showNoisy) {
      return normal;
    }
    return [...normal, ...(this.noisyEventsByRole.get(role) ?? [])];
  }

  private bufferFor(role: string) {
    let buffer = this.eventsByRole.get(role);
    if (!buffer) {
      buffer = [];
      this.eventsByRole.set(role, buffer);
    }
    return buffer;
  }

  private noisyBufferFor(role: string) {
    let buffer = this.noisyEventsByRole.get(role);
    if (!buffer) {
      buffer = [];
      this.noisyEventsByRole.set(role, buffer);
    }
    return buffer;
  }
}
