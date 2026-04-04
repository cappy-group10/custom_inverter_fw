import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";

import { api } from "../lib/api";
import { createEmptySnapshot, derivePrimaryMcuDetail, derivePrimaryMcuSummary, mergeStreamEvent } from "../lib/selectors";
import type { McuDetail, McuSummary, PortOption, SessionSnapshot, StartSessionPayload } from "../lib/types";

interface DashboardContextValue {
  snapshot: SessionSnapshot;
  ports: PortOption[];
  wsConnected: boolean;
  loading: {
    ports: boolean;
    session: boolean;
    brake: boolean;
  };
  error: string | null;
  primaryMcu: McuSummary | null;
  primaryMcuDetail: McuDetail | null;
  reloadPorts: () => Promise<void>;
  refreshState: () => Promise<void>;
  startSession: (payload: StartSessionPayload) => Promise<void>;
  stopSession: () => Promise<void>;
  engageBrake: () => Promise<void>;
  clearError: () => void;
}

const DashboardContext = createContext<DashboardContextValue | null>(null);

export function DashboardProvider({ children }: { children: ReactNode }) {
  const [snapshot, setSnapshot] = useState<SessionSnapshot>(createEmptySnapshot());
  const [ports, setPorts] = useState<PortOption[]>([]);
  const [wsConnected, setWsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState({ ports: false, session: false, brake: false });

  const reconnectTimerRef = useRef<number | null>(null);
  const chartSampleMsRef = useRef(0);

  const reloadPorts = async () => {
    setLoading((current) => ({ ...current, ports: true }));
    try {
      setError(null);
      setPorts(await api.getPorts());
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load serial ports");
    } finally {
      setLoading((current) => ({ ...current, ports: false }));
    }
  };

  const refreshState = async () => {
    try {
      setError(null);
      const nextSnapshot = await api.getState();
      chartSampleMsRef.current = 0;
      setSnapshot(nextSnapshot);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load dashboard state");
    }
  };

  const startSession = async (payload: StartSessionPayload) => {
    setLoading((current) => ({ ...current, session: true }));
    try {
      setError(null);
      const nextSnapshot = await api.startSession(payload);
      setSnapshot(nextSnapshot);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to start the session");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, session: false }));
    }
  };

  const stopSession = async () => {
    setLoading((current) => ({ ...current, session: true }));
    try {
      setError(null);
      const nextSnapshot = await api.stopSession();
      setSnapshot(nextSnapshot);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to stop the session");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, session: false }));
    }
  };

  const engageBrake = async () => {
    setLoading((current) => ({ ...current, brake: true }));
    try {
      setError(null);
      const detail = await api.engageBrake("primary");
      setSnapshot((current) => ({
        ...current,
        active_override: detail.active_override,
        last_host_command: detail.command ?? current.last_host_command,
        latest_mcu_status: detail.status ?? current.latest_mcu_status,
      }));
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to engage brake override");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, brake: false }));
    }
  };

  useEffect(() => {
    void reloadPorts();
    void refreshState();
  }, []);

  useEffect(() => {
    const connect = () => {
      const protocol = window.location.protocol === "https:" ? "wss" : "ws";
      const socket = new WebSocket(`${protocol}://${window.location.host}/api/stream`);

      socket.addEventListener("open", () => {
        setWsConnected(true);
      });

      socket.addEventListener("message", (event) => {
        const parsed = JSON.parse(event.data) as { type: string; payload: unknown };
        setSnapshot((current) => mergeStreamEvent(current, parsed.type, parsed.payload, chartSampleMsRef));
      });

      socket.addEventListener("error", () => {
        setWsConnected(false);
      });

      socket.addEventListener("close", () => {
        setWsConnected(false);
        if (reconnectTimerRef.current !== null) {
          window.clearTimeout(reconnectTimerRef.current);
        }
        reconnectTimerRef.current = window.setTimeout(connect, 1500);
      });

      return socket;
    };

    const socket = connect();
    return () => {
      socket.close();
      if (reconnectTimerRef.current !== null) {
        window.clearTimeout(reconnectTimerRef.current);
      }
    };
  }, []);

  const value = useMemo<DashboardContextValue>(
    () => ({
      snapshot,
      ports,
      wsConnected,
      loading,
      error,
      primaryMcu: derivePrimaryMcuSummary(snapshot),
      primaryMcuDetail: derivePrimaryMcuDetail(snapshot),
      reloadPorts,
      refreshState,
      startSession,
      stopSession,
      engageBrake,
      clearError: () => setError(null),
    }),
    [snapshot, ports, wsConnected, loading, error],
  );

  return <DashboardContext.Provider value={value}>{children}</DashboardContext.Provider>;
}

export function useDashboard() {
  const value = useContext(DashboardContext);
  if (!value) {
    throw new Error("useDashboard must be used inside DashboardProvider");
  }
  return value;
}

export { DashboardContext, type DashboardContextValue };
