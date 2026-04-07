import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { useLocation } from "react-router-dom";

import { api } from "../lib/api";
import { frontendLogger } from "../lib/frontendLogger";
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
    music: boolean;
  };
  error: string | null;
  primaryMcu: McuSummary | null;
  primaryMcuDetail: McuDetail | null;
  reloadPorts: () => Promise<void>;
  refreshState: () => Promise<void>;
  startSession: (payload: StartSessionPayload) => Promise<void>;
  stopSession: () => Promise<void>;
  engageBrake: () => Promise<void>;
  releaseBrake: () => Promise<void>;
  playMusic: (songId: number, amplitude?: number) => Promise<void>;
  pauseMusic: () => Promise<void>;
  resumeMusic: () => Promise<void>;
  stopMusic: () => Promise<void>;
  setMusicVolume: (volume: number) => Promise<void>;
  clearError: () => void;
}

const DashboardContext = createContext<DashboardContextValue | null>(null);

export function DashboardProvider({ children }: { children: ReactNode }) {
  const location = useLocation();
  const [snapshot, setSnapshot] = useState<SessionSnapshot>(createEmptySnapshot());
  const [ports, setPorts] = useState<PortOption[]>([]);
  const [wsConnected, setWsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState({ ports: false, session: false, brake: false, music: false });

  const reconnectTimerRef = useRef<number | null>(null);
  const chartSampleMsRef = useRef(0);

  const reloadPorts = async () => {
    setLoading((current) => ({ ...current, ports: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Loading UART port list");
      setPorts(await api.getPorts());
      frontendLogger.info("frontend", "UART port list loaded");
    } catch (err) {
      frontendLogger.error("frontend", "UART port list failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
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
      frontendLogger.info("frontend", "Dashboard state refreshed", {
        session_state: nextSnapshot.session_state,
      });
    } catch (err) {
      frontendLogger.error("frontend", "Dashboard state refresh failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
      setError(err instanceof Error ? err.message : "Unable to load dashboard state");
    }
  };

  const startSession = async (payload: StartSessionPayload) => {
    setLoading((current) => ({ ...current, session: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Session start requested", payload);
      const nextSnapshot = await api.startSession(payload);
      setSnapshot(nextSnapshot);
      frontendLogger.info("frontend", "Session start succeeded", {
        session_state: nextSnapshot.session_state,
        port: nextSnapshot.port || "demo",
      });
    } catch (err) {
      frontendLogger.error("frontend", "Session start failed", {
        error: err instanceof Error ? err.message : "Unknown error",
        port: payload.port || "demo",
      });
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
      frontendLogger.info("frontend", "Session stop requested");
      const nextSnapshot = await api.stopSession();
      setSnapshot(nextSnapshot);
      frontendLogger.info("frontend", "Session stop succeeded", {
        session_state: nextSnapshot.session_state,
      });
    } catch (err) {
      frontendLogger.error("frontend", "Session stop failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
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
      frontendLogger.warn("frontend", "Emergency brake requested", { mcu_id: "primary" });
      const detail = await api.engageBrake("primary");
      setSnapshot((current) => ({
        ...current,
        active_override: detail.active_override,
        motor_config: detail.motor_config ?? current.motor_config,
        last_host_command: detail.command ?? current.last_host_command,
        latest_mcu_status: detail.status ?? current.latest_mcu_status,
      }));
      frontendLogger.warn("frontend", "Emergency brake latched", { mcu_id: "primary" });
    } catch (err) {
      frontendLogger.error("frontend", "Emergency brake failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
      setError(err instanceof Error ? err.message : "Unable to engage brake override");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, brake: false }));
    }
  };

  const releaseBrake = async () => {
    setLoading((current) => ({ ...current, brake: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Brake release requested", { mcu_id: "primary" });
      const detail = await api.releaseBrake("primary");
      setSnapshot((current) => ({
        ...current,
        active_override: detail.active_override,
        motor_config: detail.motor_config ?? current.motor_config,
        last_host_command: detail.command ?? current.last_host_command,
        latest_mcu_status: detail.status ?? current.latest_mcu_status,
      }));
      frontendLogger.info("frontend", "Brake released", { mcu_id: "primary" });
    } catch (err) {
      frontendLogger.error("frontend", "Brake release failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
      setError(err instanceof Error ? err.message : "Unable to release brake override");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, brake: false }));
    }
  };

  const playMusic = async (songId: number, amplitude?: number) => {
    setLoading((current) => ({ ...current, music: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Music play requested", { song_id: songId, amplitude });
      const nextSnapshot = await api.playMusic(songId, amplitude);
      setSnapshot(nextSnapshot);
      frontendLogger.info("frontend", "Music play succeeded", {
        song_id: songId,
        play_state: nextSnapshot.music_state?.play_state || null,
      });
    } catch (err) {
      frontendLogger.error("frontend", "Music play failed", {
        error: err instanceof Error ? err.message : "Unknown error",
        song_id: songId,
      });
      setError(err instanceof Error ? err.message : "Unable to start music playback");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, music: false }));
    }
  };

  const pauseMusic = async () => {
    setLoading((current) => ({ ...current, music: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Music pause requested");
      const nextSnapshot = await api.pauseMusic();
      setSnapshot(nextSnapshot);
    } catch (err) {
      frontendLogger.error("frontend", "Music pause failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
      setError(err instanceof Error ? err.message : "Unable to pause music playback");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, music: false }));
    }
  };

  const resumeMusic = async () => {
    setLoading((current) => ({ ...current, music: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Music resume requested");
      const nextSnapshot = await api.resumeMusic();
      setSnapshot(nextSnapshot);
    } catch (err) {
      frontendLogger.error("frontend", "Music resume failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
      setError(err instanceof Error ? err.message : "Unable to resume music playback");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, music: false }));
    }
  };

  const stopMusic = async () => {
    setLoading((current) => ({ ...current, music: true }));
    try {
      setError(null);
      frontendLogger.info("frontend", "Music stop requested");
      const nextSnapshot = await api.stopMusic();
      setSnapshot(nextSnapshot);
    } catch (err) {
      frontendLogger.error("frontend", "Music stop failed", {
        error: err instanceof Error ? err.message : "Unknown error",
      });
      setError(err instanceof Error ? err.message : "Unable to stop music playback");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, music: false }));
    }
  };

  const setMusicVolume = async (volume: number) => {
    setLoading((current) => ({ ...current, music: true }));
    try {
      setError(null);
      const nextSnapshot = await api.setMusicVolume(volume);
      setSnapshot(nextSnapshot);
    } catch (err) {
      frontendLogger.error("frontend", "Music volume update failed", {
        error: err instanceof Error ? err.message : "Unknown error",
        volume,
      });
      setError(err instanceof Error ? err.message : "Unable to update music volume");
      throw err;
    } finally {
      setLoading((current) => ({ ...current, music: false }));
    }
  };

  useEffect(() => {
    void reloadPorts();
    void refreshState();
  }, []);

  useEffect(() => {
    frontendLogger.info("route", "Route changed", {
      pathname: location.pathname,
      search: location.search,
      hash: location.hash,
    });
  }, [location.hash, location.pathname, location.search]);

  useEffect(() => {
    const connect = () => {
      const protocol = window.location.protocol === "https:" ? "wss" : "ws";
      const socket = new WebSocket(`${protocol}://${window.location.host}/api/stream`);

      socket.addEventListener("open", () => {
        setWsConnected(true);
        frontendLogger.info("websocket", "Dashboard websocket connected");
      });

      socket.addEventListener("message", (event) => {
        const parsed = JSON.parse(event.data) as { type: string; payload: unknown };
        setSnapshot((current) => mergeStreamEvent(current, parsed.type, parsed.payload, chartSampleMsRef));
      });

      socket.addEventListener("error", () => {
        setWsConnected(false);
        frontendLogger.warn("websocket", "Dashboard websocket error");
      });

      socket.addEventListener("close", () => {
        setWsConnected(false);
        frontendLogger.warn("websocket", "Dashboard websocket closed; scheduling reconnect");
        if (reconnectTimerRef.current !== null) {
          window.clearTimeout(reconnectTimerRef.current);
        }
        reconnectTimerRef.current = window.setTimeout(connect, 1500);
      });

      return socket;
    };

    const socket = connect();
    return () => {
      frontendLogger.info("websocket", "Dashboard websocket cleanup");
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
      releaseBrake,
      playMusic,
      pauseMusic,
      resumeMusic,
      stopMusic,
      setMusicVolume,
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
