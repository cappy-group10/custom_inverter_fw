import type { ConnectionInstance, SessionSnapshot } from "./types";

const STORAGE_KEY = "inverter-os.connection-instances.v1";

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

function createInstanceId() {
  const stamp = Date.now().toString(36);
  const suffix = Math.random().toString(36).slice(2, 8);
  return `instance-${stamp}-${suffix}`;
}

export function createConnectionInstance(): ConnectionInstance {
  const existing = loadConnectionInstances();
  const now = Date.now() / 1000;
  const instance: ConnectionInstance = {
    id: createInstanceId(),
    name: nextInstanceName(existing),
    created_at: now,
    last_opened_at: now,
  };
  persist([instance, ...existing]);
  return instance;
}

export function deleteConnectionInstance(instanceId: string) {
  persist(loadConnectionInstances().filter((instance) => instance.id !== instanceId));
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

export function updateConnectionInstanceFromSnapshot(instanceId: string | null | undefined, snapshot: SessionSnapshot) {
  if (!instanceId) {
    return;
  }
  const updated = loadConnectionInstances().map((instance) => {
    if (instance.id !== instanceId) {
      return instance;
    }
    return {
      ...instance,
      last_opened_at: Date.now() / 1000,
      port: snapshot.port || "demo",
      session_state: snapshot.session_state,
      controller_name: snapshot.joystick_name || null,
      last_frame_at: snapshot.health?.last_frame_at ?? null,
    };
  });
  persist(updated);
}
