import type { FrontendLogRecord, McuDetail, McuSummary, PortOption, SessionSnapshot, StartSessionPayload } from "./types";

async function fetchJSON<T>(url: string, options: RequestInit = {}): Promise<T> {
  const response = await fetch(url, {
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {}),
    },
    ...options,
  });

  if (!response.ok) {
    const payload = await response.json().catch(() => ({ detail: "Request failed" }));
    throw new Error(payload.detail || "Request failed");
  }

  return response.json() as Promise<T>;
}

export const api = {
  getPorts: async (): Promise<PortOption[]> => {
    const payload = await fetchJSON<{ ports: PortOption[] }>("/api/ports");
    return payload.ports ?? [];
  },
  getState: () => fetchJSON<SessionSnapshot>("/api/state"),
  getMcus: async (): Promise<McuSummary[]> => {
    const payload = await fetchJSON<{ mcus: McuSummary[] }>("/api/mcus");
    return payload.mcus ?? [];
  },
  getMcuDetail: (mcuId = "primary") => fetchJSON<McuDetail>(`/api/mcus/${mcuId}`),
  startSession: (payload: StartSessionPayload) =>
    fetchJSON<SessionSnapshot>("/api/session/start", {
      method: "POST",
      body: JSON.stringify(payload),
    }),
  stopSession: () =>
    fetchJSON<SessionSnapshot>("/api/session/stop", {
      method: "POST",
    }),
  engageBrake: (mcuId = "primary") =>
    fetchJSON<McuDetail>(`/api/mcus/${mcuId}/brake`, {
      method: "POST",
    }),
  releaseBrake: (mcuId = "primary") =>
    fetchJSON<McuDetail>(`/api/mcus/${mcuId}/brake/release`, {
      method: "POST",
    }),
  postFrontendLogs: (records: FrontendLogRecord[]) =>
    fetchJSON<{ accepted: number }>("/api/logs/frontend", {
      method: "POST",
      body: JSON.stringify({ records }),
    }),
};
