import { Navigate, Route, Routes } from "react-router-dom";

import { DashboardContext } from "./context/DashboardContext";
import { DashboardPage } from "./pages/DashboardPage";
import { LandingPage } from "./pages/LandingPage";
import { McuPage } from "./pages/McuPage";

export function App() {
  return (
    <Routes>
      <Route path="/" element={<LandingPage />} />
      <Route path="/configure" element={<DashboardPage />} />
      <Route path="/mcu/:mcuId" element={<McuPage />} />
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}

export { DashboardContext };
