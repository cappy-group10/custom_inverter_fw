import type { ConnectionInstance, SessionSnapshot } from "./types";

const STORAGE_KEY = "inverter-os.connection-instances.v1";
const SNAPSHOT_PERSIST_INTERVAL_MS = 1000;

type ConnectionInstanceSnapshotInput = Pick<SessionSnapshot, "session_state" | "port" | "joystick_name" | "health">;
type PersistedConnectionSnapshot = {
  port: string;
  session_state: SessionSnapshot["session_state"];
  controller_name: string | null;
  last_frame_at: number | null;
};

const lastPersistedAtByInstance = new Map<string, number>();
const pendingSnapshotByInstance = new Map<string, PersistedConnectionSnapshot>();
const flushTimerByInstance = new Map<string, number>();

function hasStorage() {
  return typeof window !== "undefined" && typeof window.localStorage !== "undefined";
}

function persist(instances: ConnectionInstance[]) {
  if (!hasStorage()) {
    return;
  }
  window.localStorage.setItem(STORAGE_KEY, JSON.stringify(instances));
}

function parseInstances(value: string | null): ConnectionInstance[] {
  if (!value) {
    return [];
  }
  try {
    const parsed = JSON.parse(value) as ConnectionInstance[];
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

export function loadConnectionInstances(): ConnectionInstance[] {
  if (!hasStorage()) {
    return [];
  }
  return parseInstances(window.localStorage.getItem(STORAGE_KEY)).sort((left, right) => right.last_opened_at - left.last_opened_at);
}

function nextInstanceName(instances: ConnectionInstance[]) {
  const maxIndex = instances.reduce((highest, instance) => {
    const match = instance.name.match(/Connection Instance (\d+)/i);
    if (!match) {
      return highest;
    }
    return Math.max(highest, Number(match[1]));
  }, 0);
  return `Connection Instance ${String(maxIndex + 1).padStart(2, "0")}`;
}

function normalizeInstanceName(name: string | null | undefined, fallback: string) {
  const trimmed = String(name || "").trim().replace(/\s+/g, " ");
  return trimmed || fallback;
}

function createInstanceId() {
  const stamp = Date.now().toString(36);
  const suffix = Math.random().toString(36).slice(2, 8);
  return `instance-${stamp}-${suffix}`;
}

function clearScheduledSnapshotFlush(instanceId: string) {
  const timer = flushTimerByInstance.get(instanceId);
  if (timer !== undefined) {
    window.clearTimeout(timer);
    flushTimerByInstance.delete(instanceId);
  }
}

function buildPersistedSnapshot(snapshot: ConnectionInstanceSnapshotInput): PersistedConnectionSnapshot {
  return {
    port: snapshot.port || "demo",
    session_state: snapshot.session_state,
    controller_name: snapshot.joystick_name || null,
    last_frame_at: snapshot.health?.last_frame_at ?? null,
  };
}

function hasMaterialSnapshotChange(instance: ConnectionInstance, nextSnapshot: PersistedConnectionSnapshot) {
  return (
    (instance.port || "demo") !== nextSnapshot.port ||
    (instance.session_state || null) !== nextSnapshot.session_state ||
    (instance.controller_name || null) !== nextSnapshot.controller_name ||
    (instance.last_frame_at ?? null) !== nextSnapshot.last_frame_at
  );
}

function persistConnectionSnapshot(instanceId: string, nextSnapshot: PersistedConnectionSnapshot) {
  const existing = loadConnectionInstances();
  let changed = false;
  const updated = existing.map((instance) => {
    if (instance.id !== instanceId) {
      return instance;
    }
    if (!hasMaterialSnapshotChange(instance, nextSnapshot)) {
      return instance;
    }
    changed = true;
    return {
      ...instance,
      port: nextSnapshot.port,
      session_state: nextSnapshot.session_state,
      controller_name: nextSnapshot.controller_name,
      last_frame_at: nextSnapshot.last_frame_at,
    };
  });

  if (changed) {
    persist(updated);
    lastPersistedAtByInstance.set(instanceId, Date.now());
  }

  pendingSnapshotByInstance.delete(instanceId);
  clearScheduledSnapshotFlush(instanceId);
}

function scheduleSnapshotPersistence(instanceId: string, nextSnapshot: PersistedConnectionSnapshot, delayMs: number) {
  pendingSnapshotByInstance.set(instanceId, nextSnapshot);
  if (flushTimerByInstance.has(instanceId) || !hasStorage()) {
    return;
  }

  const timer = window.setTimeout(() => {
    const pending = pendingSnapshotByInstance.get(instanceId);
    if (!pending) {
      clearScheduledSnapshotFlush(instanceId);
      return;
    }
    persistConnectionSnapshot(instanceId, pending);
  }, Math.max(0, delayMs));

  flushTimerByInstance.set(instanceId, timer);
}

export function createConnectionInstance(name?: string): ConnectionInstance {
  const existing = loadConnectionInstances();
  const now = Date.now() / 1000;
  const fallbackName = nextInstanceName(existing);
  const instance: ConnectionInstance = {
    id: createInstanceId(),
    name: normalizeInstanceName(name, fallbackName),
    created_at: now,
    last_opened_at: now,
  };
  persist([instance, ...existing]);
  lastPersistedAtByInstance.set(instance.id, Date.now());
  return instance;
}

export function deleteConnectionInstance(instanceId: string) {
  pendingSnapshotByInstance.delete(instanceId);
  lastPersistedAtByInstance.delete(instanceId);
  clearScheduledSnapshotFlush(instanceId);
  persist(loadConnectionInstances().filter((instance) => instance.id !== instanceId));
}

export function renameConnectionInstance(instanceId: string, name: string): ConnectionInstance | null {
  let renamedInstance: ConnectionInstance | null = null;
  const updated = loadConnectionInstances().map((instance) => {
    if (instance.id !== instanceId) {
      return instance;
    }
    renamedInstance = {
      ...instance,
      name: normalizeInstanceName(name, instance.name),
    };
    return renamedInstance;
  });
  persist(updated);
  if (renamedInstance) {
    lastPersistedAtByInstance.set(instanceId, Date.now());
  }
  return renamedInstance;
}

export function markConnectionInstanceOpened(instanceId: string): ConnectionInstance | null {
  const now = Date.now() / 1000;
  let updatedInstance: ConnectionInstance | null = null;
  const updated = loadConnectionInstances().map((instance) => {
    if (instance.id !== instanceId) {
      return instance;
    }
    updatedInstance = {
      ...instance,
      last_opened_at: now,
    };
    return updatedInstance;
  });
  persist(updated);
  if (updatedInstance) {
    lastPersistedAtByInstance.set(instanceId, Date.now());
  }
  return updatedInstance;
}

export function getConnectionInstance(instanceId: string | null | undefined): ConnectionInstance | null {
  if (!instanceId) {
    return null;
  }
  return loadConnectionInstances().find((instance) => instance.id === instanceId) || null;
}

export function getMostRecentlyOpenedInstance(): ConnectionInstance | null {
  return loadConnectionInstances()[0] || null;
}

export function updateConnectionInstanceFromSnapshot(instanceId: string | null | undefined, snapshot: ConnectionInstanceSnapshotInput) {
  if (!instanceId) {
    return;
  }
  const currentInstance = getConnectionInstance(instanceId);
  if (!currentInstance) {
    pendingSnapshotByInstance.delete(instanceId);
    clearScheduledSnapshotFlush(instanceId);
    return;
  }

  const nextSnapshot = buildPersistedSnapshot(snapshot);
  if (!hasMaterialSnapshotChange(currentInstance, nextSnapshot)) {
    pendingSnapshotByInstance.delete(instanceId);
    clearScheduledSnapshotFlush(instanceId);
    return;
  }

  const now = Date.now();
  const lastPersistedAt = lastPersistedAtByInstance.get(instanceId) ?? 0;
  const elapsedMs = now - lastPersistedAt;

  if (elapsedMs >= SNAPSHOT_PERSIST_INTERVAL_MS) {
    persistConnectionSnapshot(instanceId, nextSnapshot);
    return;
  }

  scheduleSnapshotPersistence(instanceId, nextSnapshot, SNAPSHOT_PERSIST_INTERVAL_MS - elapsedMs);
}
