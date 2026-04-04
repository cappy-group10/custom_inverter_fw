import { useEffect, useMemo, useRef, useState } from "react";

import { InfoHint, UiIcon } from "./UiChrome";
import {
  compactDecoded,
  compactRawHex,
  formatTimestamp,
  getAsciiPreview,
  getRawBytes,
} from "../lib/selectors";
import type { FrameRecord, SessionSnapshot } from "../lib/types";

interface UartDebugTerminalProps {
  snapshot: SessionSnapshot;
}

const TERMINAL_ROW_LIMIT = 200;

function createFrameKey(frame: FrameRecord) {
  return [
    Number(frame.timestamp || 0).toFixed(6),
    frame.direction || "?",
    frame.frame_id || 0,
    frame.raw_hex || "",
  ].join("|");
}

function renderByteTokens(bytes: number[], tokenClass = "") {
  if (!bytes.length) {
    return <span className="mono-muted">none</span>;
  }
  return bytes.map((byte, index) => (
    <span key={`${tokenClass || "payload"}-${index}-${byte}`} className={`byte-token ${tokenClass}`.trim()}>
      {byte.toString(16).padStart(2, "0")}
    </span>
  ));
}

export function UartDebugTerminal({ snapshot }: UartDebugTerminalProps) {
  const [showTx, setShowTx] = useState(true);
  const [showRx, setShowRx] = useState(true);
  const [errorsOnly, setErrorsOnly] = useState(false);
  const [search, setSearch] = useState("");
  const [paused, setPaused] = useState(false);
  const [autoScroll, setAutoScroll] = useState(true);
  const [selectedFrameKey, setSelectedFrameKey] = useState<string | null>(null);
  const [frozenFrames, setFrozenFrames] = useState<FrameRecord[] | null>(null);
  const [clearedBefore, setClearedBefore] = useState<number | null>(null);
  const terminalRef = useRef<HTMLDivElement | null>(null);

  const sourceFrames = useMemo(() => {
    const baseFrames = paused && frozenFrames ? frozenFrames : snapshot.recent_frames || [];
    if (clearedBefore === null) {
      return baseFrames;
    }
    return baseFrames.filter((frame) => frame.timestamp > clearedBefore);
  }, [clearedBefore, frozenFrames, paused, snapshot.recent_frames]);

  const filteredFrames = useMemo(() => {
    const query = search.trim().toLowerCase();
    return sourceFrames.filter((frame) => {
      const direction = String(frame.direction || "").toLowerCase();
      if (direction === "tx" && !showTx) {
        return false;
      }
      if (direction === "rx" && !showRx) {
        return false;
      }
      if (errorsOnly && frame.checksum_ok && !frame.decoded?.error) {
        return false;
      }
      if (!query) {
        return true;
      }
      if (String(frame.frame_name || "").toLowerCase().includes(query)) {
        return true;
      }
      if (String(frame.frame_id ?? "").toLowerCase().includes(query)) {
        return true;
      }
      if (String(frame.raw_hex || "").toLowerCase().includes(query)) {
        return true;
      }
      return JSON.stringify(frame.decoded || {}).toLowerCase().includes(query);
    });
  }, [errorsOnly, search, showRx, showTx, sourceFrames]);

  const visibleFrames = useMemo(
    () => filteredFrames.slice(Math.max(filteredFrames.length - TERMINAL_ROW_LIMIT, 0)),
    [filteredFrames],
  );

  const selectedFrame = useMemo(
    () => filteredFrames.find((frame) => createFrameKey(frame) === selectedFrameKey) || null,
    [filteredFrames, selectedFrameKey],
  );

  useEffect(() => {
    if (selectedFrameKey && !selectedFrame) {
      setSelectedFrameKey(null);
    }
  }, [selectedFrame, selectedFrameKey]);

  useEffect(() => {
    if (!autoScroll || paused) {
      return;
    }
    const surface = terminalRef.current;
    if (!surface) {
      return;
    }
    surface.scrollTop = surface.scrollHeight;
  }, [autoScroll, paused, visibleFrames.length]);

  return (
    <section className="panel uart-debug-panel wide-panel">
      <div className="panel-heading">
        <div>
          <div className="heading-line">
            <UiIcon name="uart" className="heading-icon" />
            <h2>UART Debug Terminal</h2>
            <InfoHint text="Inspect the live serial stream with TX/RX filtering, searchable frame previews, and a decoded inspector for the selected packet." />
          </div>
          <p className="panel-copy">A terminal-style TX/RX stream with raw bytes, decoded payloads, and per-frame inspection.</p>
        </div>
        <div className="button-row compact">
          <label className="inline-toggle">
            <input type="checkbox" checked={autoScroll} onChange={(event) => setAutoScroll(event.target.checked)} />
            <span>Auto-scroll</span>
          </label>
          <button
            onClick={() => {
              if (!paused) {
                setFrozenFrames(sourceFrames);
              } else {
                setFrozenFrames(null);
              }
              setPaused((current) => !current);
            }}
          >
            <UiIcon name={paused ? "open" : "events"} />
            {paused ? "Resume" : "Pause"}
          </button>
          <button
            onClick={() => {
              setClearedBefore(sourceFrames[sourceFrames.length - 1]?.timestamp ?? Date.now() / 1000);
              if (paused) {
                setFrozenFrames([]);
              }
              setSelectedFrameKey(null);
            }}
          >
            <UiIcon name="delete" />
            Clear
          </button>
        </div>
      </div>

      <div className="uart-summary-grid">
        <div className="uart-summary-card"><span>Port</span><strong>{snapshot.port || "demo"}</strong></div>
        <div className="uart-summary-card"><span>Baud</span><strong>{String(snapshot.baudrate || 115200)}</strong></div>
        <div className="uart-summary-card"><span>TX</span><strong>{snapshot.counters.tx_frames ?? 0}</strong></div>
        <div className="uart-summary-card"><span>RX</span><strong>{snapshot.counters.rx_frames ?? 0}</strong></div>
        <div className="uart-summary-card"><span>Checksum Errors</span><strong>{snapshot.counters.checksum_errors ?? 0}</strong></div>
        <div className="uart-summary-card"><span>Serial Errors</span><strong>{snapshot.counters.serial_errors ?? 0}</strong></div>
        <div className="uart-summary-card wide"><span>Last Frame</span><strong>{formatTimestamp(snapshot.health.last_frame_at)}</strong></div>
      </div>

      <div className="uart-toolbar">
        <div className="uart-filter-group">
          <label className="inline-toggle">
            <input type="checkbox" checked={showTx} onChange={(event) => setShowTx(event.target.checked)} />
            <span>TX</span>
          </label>
          <label className="inline-toggle">
            <input type="checkbox" checked={showRx} onChange={(event) => setShowRx(event.target.checked)} />
            <span>RX</span>
          </label>
          <label className="inline-toggle">
            <input type="checkbox" checked={errorsOnly} onChange={(event) => setErrorsOnly(event.target.checked)} />
            <span>Errors only</span>
          </label>
        </div>
        <label className="uart-search-field">
          <span>Search</span>
          <input value={search} onChange={(event) => setSearch(event.target.value)} type="search" placeholder="frame name, hex, or decoded JSON" />
        </label>
      </div>

      <div className="uart-debug-layout">
        <section className="uart-terminal-panel">
          <div className="uart-terminal-header">
            <span>Live terminal stream</span>
            <span className="mono-muted">newest at bottom</span>
          </div>
          <div ref={terminalRef} className="uart-terminal">
            {visibleFrames.length ? (
              visibleFrames.map((frame, index) => {
                const previousFrame = visibleFrames[index - 1];
                const deltaMs = previousFrame ? Math.max(0, (frame.timestamp - previousFrame.timestamp) * 1000) : null;
                const rawBytes = getRawBytes(frame.raw_hex);
                const checksumLabel = frame.checksum_ok ? "Checksum OK" : "Checksum error";
                const direction = String(frame.direction || "?").toLowerCase();
                const frameKey = createFrameKey(frame);
                const hasError = !frame.checksum_ok || Boolean(frame.decoded?.error);

                return (
                  <button
                    key={frameKey}
                    type="button"
                    className={`uart-line ${direction}${hasError ? " error" : ""}${selectedFrameKey === frameKey ? " selected" : ""}`}
                    data-frame-key={frameKey}
                    onClick={() => setSelectedFrameKey(frameKey)}
                  >
                    <div className="uart-line-meta">
                      <div className="uart-line-header">
                        <span className="uart-line-title">{frame.frame_name || `frame_${frame.frame_id}`}</span>
                        <span className={`uart-line-badge ${direction}`}>{direction.toUpperCase()}</span>
                        <span className={`uart-line-badge ${frame.checksum_ok ? "ok" : "error"}`}>{checksumLabel}</span>
                        <span className="uart-line-badge">{`0x${Number(frame.frame_id || 0).toString(16).padStart(2, "0")}`}</span>
                        <span className="uart-line-badge">{`${rawBytes.length} bytes`}</span>
                      </div>
                      <div className="uart-line-time">
                        {`${formatTimestamp(frame.timestamp)}${deltaMs === null ? "" : ` · +${deltaMs.toFixed(1)} ms`}`}
                      </div>
                    </div>
                    <div className="uart-line-preview raw">{compactRawHex(frame.raw_hex)}</div>
                    <div className="uart-line-preview decoded">{compactDecoded(frame.decoded)}</div>
                  </button>
                );
              })
            ) : (
              <div className="placeholder">
                {snapshot.recent_frames.length ? "No UART frames match the current filters." : "No UART activity yet."}
              </div>
            )}
          </div>
        </section>

        <aside className="uart-inspector-panel">
          <div className="uart-terminal-header">
            <span>Selected frame</span>
            <span className="mono-muted">decoded + raw</span>
          </div>
          <div className="uart-inspector">
            {selectedFrame ? (
              <>
                <div className="uart-inspector-meta">
                  <article className="uart-inspector-card">
                    <span>Direction</span>
                    <strong>{String(selectedFrame.direction || "").toUpperCase()}</strong>
                  </article>
                  <article className="uart-inspector-card">
                    <span>Frame</span>
                    <strong>{`${selectedFrame.frame_name || "frame"} (0x${Number(selectedFrame.frame_id || 0).toString(16).padStart(2, "0")})`}</strong>
                  </article>
                  <article className="uart-inspector-card">
                    <span>Timestamp</span>
                    <strong>{formatTimestamp(selectedFrame.timestamp)}</strong>
                  </article>
                  <article className="uart-inspector-card">
                    <span>Status</span>
                    <strong>{selectedFrame.checksum_ok ? "Checksum OK" : "Checksum error"}</strong>
                  </article>
                </div>

                <article className="uart-inspector-block">
                  <h3>Byte Breakdown</h3>
                  <div className="byte-breakdown">
                    <div className="byte-breakdown-row">
                      <span>Sync</span>
                      <div className="byte-segment">{renderByteTokens(getRawBytes(selectedFrame.raw_hex).slice(0, 1), "sync")}</div>
                    </div>
                    <div className="byte-breakdown-row">
                      <span>Frame ID</span>
                      <div className="byte-segment">{renderByteTokens(getRawBytes(selectedFrame.raw_hex).slice(1, 2), "frame-id")}</div>
                    </div>
                    <div className="byte-breakdown-row">
                      <span>Payload</span>
                      <div className="byte-segment">{renderByteTokens(getRawBytes(selectedFrame.raw_hex).slice(2, -1))}</div>
                    </div>
                    <div className="byte-breakdown-row">
                      <span>Checksum</span>
                      <div className="byte-segment">{renderByteTokens(getRawBytes(selectedFrame.raw_hex).slice(-1), "checksum")}</div>
                    </div>
                  </div>
                </article>

                <article className="uart-inspector-block">
                  <h3>Decoded JSON</h3>
                  <pre>{JSON.stringify(selectedFrame.decoded || {}, null, 2)}</pre>
                </article>

                <article className="uart-inspector-block">
                  <h3>Raw Hex</h3>
                  <pre>{selectedFrame.raw_hex || ""}</pre>
                </article>

                <article className="uart-inspector-block">
                  <h3>ASCII Preview</h3>
                  <pre>{getAsciiPreview(selectedFrame.raw_hex) || "(non-printable)"}</pre>
                </article>
              </>
            ) : (
              <div className="placeholder">Select a UART frame to inspect its decoded fields and byte layout.</div>
            )}
          </div>
        </aside>
      </div>
    </section>
  );
}
