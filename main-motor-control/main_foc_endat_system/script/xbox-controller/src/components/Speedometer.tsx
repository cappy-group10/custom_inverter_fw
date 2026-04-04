interface SpeedometerProps {
  value: number;
  min?: number;
  max?: number;
}

function polarToCartesian(centerX: number, centerY: number, radius: number, angleInDegrees: number) {
  const angleInRadians = ((angleInDegrees - 90) * Math.PI) / 180;
  return {
    x: centerX + radius * Math.cos(angleInRadians),
    y: centerY + radius * Math.sin(angleInRadians),
  };
}

function describeArc(centerX: number, centerY: number, radius: number, startAngle: number, endAngle: number) {
  const start = polarToCartesian(centerX, centerY, radius, endAngle);
  const end = polarToCartesian(centerX, centerY, radius, startAngle);
  const largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";
  return ["M", start.x, start.y, "A", radius, radius, 0, largeArcFlag, 0, end.x, end.y].join(" ");
}

export function Speedometer({ value, min = -0.3, max = 0.3 }: SpeedometerProps) {
  const clamped = Math.max(min, Math.min(max, value));
  const ratio = (clamped - min) / (max - min);
  const angle = -120 + ratio * 240;
  const needle = polarToCartesian(160, 160, 98, angle);

  return (
    <div className="speedometer-card" data-testid="speedometer">
      <div className="speedometer-caption">
        <p className="eyebrow">Motor Page</p>
        <h2>Reference speed gauge</h2>
        <p className="panel-copy">Car-style dial showing the current per-unit speed reference being driven through the MCU session.</p>
      </div>

      <svg className="speedometer-svg" viewBox="0 0 320 220" role="img" aria-label="Reference speed gauge">
        <path d={describeArc(160, 160, 104, -120, 120)} className="speedometer-track" />
        <path d={describeArc(160, 160, 104, -120, angle)} className="speedometer-progress" />
        <line x1="160" y1="160" x2={needle.x} y2={needle.y} className="speedometer-needle" />
        <circle cx="160" cy="160" r="10" className="speedometer-pivot" />
        <text x="160" y="120" textAnchor="middle" className="speedometer-value">
          {clamped.toFixed(3)}
        </text>
        <text x="160" y="142" textAnchor="middle" className="speedometer-unit">
          per-unit speed_ref
        </text>
        <text x="44" y="182" textAnchor="middle" className="speedometer-tick">
          {min.toFixed(2)}
        </text>
        <text x="160" y="36" textAnchor="middle" className="speedometer-tick">
          0.00
        </text>
        <text x="276" y="182" textAnchor="middle" className="speedometer-tick">
          {max.toFixed(2)}
        </text>
      </svg>
    </div>
  );
}
