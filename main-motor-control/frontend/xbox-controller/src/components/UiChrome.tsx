import type { ReactNode } from "react";

export type IconName =
  | "info"
  | "instances"
  | "create"
  | "open"
  | "delete"
  | "edit"
  | "save"
  | "cancel"
  | "overview"
  | "controller"
  | "telemetry"
  | "motor"
  | "music"
  | "uart"
  | "events"
  | "session"
  | "transport"
  | "command"
  | "charts"
  | "back"
  | "port"
  | "controller-pad"
  | "clock"
  | "shield";

interface UiIconProps {
  name: IconName;
  size?: number;
  className?: string;
}

function iconPath(name: IconName): ReactNode {
  switch (name) {
    case "info":
      return (
        <>
          <circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <path d="M12 10.2v6.2" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <circle cx="12" cy="7.2" r="1.2" fill="currentColor" />
        </>
      );
    case "instances":
      return (
        <>
          <rect x="4" y="5" width="7" height="6" rx="1.5" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <rect x="13" y="5" width="7" height="6" rx="1.5" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <rect x="8.5" y="13" width="7" height="6" rx="1.5" fill="none" stroke="currentColor" strokeWidth="1.6" />
        </>
      );
    case "create":
      return (
        <>
          <path d="M12 6v12" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <path d="M6 12h12" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
        </>
      );
    case "open":
      return (
        <>
          <path d="M8 16 16 8" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <path d="M10 8h6v6" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
          <path d="M6 10v8h8" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "delete":
      return (
        <>
          <path d="M7 8v10M12 8v10M17 8v10" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" />
          <path d="M5 6h14" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <path d="M9 4h6" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
        </>
      );
    case "edit":
      return (
        <>
          <path d="m7 17 1.2-3.8L15.8 5.6a1.6 1.6 0 0 1 2.3 0l.3.3a1.6 1.6 0 0 1 0 2.3l-7.6 7.6Z" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinejoin="round" />
          <path d="M6.6 17.4 10 16l8-8" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" />
        </>
      );
    case "save":
      return (
        <>
          <path d="M6 5h10l2 2v12H6Z" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinejoin="round" />
          <path d="M9 5v5h6V5" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinejoin="round" />
          <rect x="9" y="14" width="6" height="3" rx="1" fill="currentColor" opacity="0.32" />
        </>
      );
    case "cancel":
      return (
        <>
          <path d="m8 8 8 8M16 8l-8 8" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
        </>
      );
    case "overview":
      return (
        <>
          <rect x="5" y="5" width="14" height="14" rx="2" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <path d="M9 10h6M9 14h6" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
        </>
      );
    case "controller":
    case "controller-pad":
      return (
        <>
          <path d="M8 9.5c-1.8 0-3 1.3-3.4 3L4 15c-.4 1.7.7 3 2.3 3H8l1.8-2h4.4L16 18h1.7c1.6 0 2.7-1.3 2.3-3l-.6-2.5c-.4-1.7-1.6-3-3.4-3H8Z" fill="none" stroke="currentColor" strokeWidth="1.4" strokeLinejoin="round" />
          <path d="M8.6 12.2v2.6M7.3 13.5h2.6M15.7 12.7h.01M17.1 14.1h.01" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
        </>
      );
    case "telemetry":
      return (
        <>
          <path d="M5 16h14" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" />
          <path d="M6 15 9.2 10l2.6 3.3 3.2-5.3L18 12" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "motor":
      return (
        <>
          <path d="M6.5 16a7 7 0 1 1 11 0" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
          <path d="M12 12 16.2 9.4" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <circle cx="12" cy="12" r="1.6" fill="currentColor" />
        </>
      );
    case "music":
      return (
        <>
          <path d="M15 6v8.2a2.8 2.8 0 1 1-1.6-2.5V8.5l-4 1V17a2.8 2.8 0 1 1-1.6-2.5V7.2l7.2-1.6Z" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "uart":
      return (
        <>
          <path d="M7 8h6M7 12h10M7 16h8" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <path d="m15 6 3 3-3 3" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "events":
      return (
        <>
          <path d="M6 7h12v11H6z" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <path d="M8.5 4.8v3.1M15.5 4.8v3.1M6 10h12" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
        </>
      );
    case "session":
      return (
        <>
          <rect x="5" y="6" width="14" height="12" rx="2" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <path d="M9 10h6M9 14h4" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
        </>
      );
    case "transport":
      return (
        <>
          <path d="M5 9h5l2-3 2 3h5" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" />
          <path d="M5 15h5l2 3 2-3h5" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "command":
      return (
        <>
          <path d="M6 8h12M6 12h8M6 16h10" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
        </>
      );
    case "charts":
      return (
        <>
          <path d="M6 17V7M12 17v-5M18 17V9" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          <path d="M6 7.5 12 12.5 18 9.5" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
        </>
      );
    case "back":
      return (
        <>
          <path d="M16 6 8 12l8 6" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "port":
      return (
        <>
          <rect x="7" y="5" width="10" height="14" rx="2" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <path d="M10 9h4M10 13h4" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
        </>
      );
    case "clock":
      return (
        <>
          <circle cx="12" cy="12" r="8.5" fill="none" stroke="currentColor" strokeWidth="1.6" />
          <path d="M12 7.6v4.8l3 1.8" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
        </>
      );
    case "shield":
      return (
        <>
          <path d="M12 4.8 18 7v4.7c0 3.4-2.3 6.2-6 7.5-3.7-1.3-6-4.1-6-7.5V7l6-2.2Z" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinejoin="round" />
        </>
      );
  }
}

export function UiIcon({ name, size = 16, className = "" }: UiIconProps) {
  return (
    <svg
      className={`ui-icon ${className}`.trim()}
      width={size}
      height={size}
      viewBox="0 0 24 24"
      aria-hidden="true"
      focusable="false"
    >
      {iconPath(name)}
    </svg>
  );
}

export function InfoHint({ text, interactive = true }: { text: string; interactive?: boolean }) {
  return (
    <span className="info-hint" tabIndex={interactive ? 0 : -1} aria-label={interactive ? text : undefined}>
      <UiIcon name="info" size={14} className="info-hint-icon" />
      <span className="info-hint-bubble">{text}</span>
    </span>
  );
}
