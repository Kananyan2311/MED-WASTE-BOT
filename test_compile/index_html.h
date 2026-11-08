#pragma once
#include <Arduino.h>
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Med-Waste Control</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Fredoka:wght@500;600;700&family=Nunito:wght@500;600;700;800&family=JetBrains+Mono:wght@600;700&display=swap" rel="stylesheet">
<style>
  :root {
    --bg1: #fff7fb;            /* near-white pink */
    --bg2: #ffe8f3;            /* powder pink */
    --text: #1a1a1a;           /* soft plum */
    --text-muted: #5f4b5c;
    --border: #000000;         /* light cotton-candy pink */
    --accent: #ff8fc2;         /* candy pink */
    --accent-hover: #ff74b4;
    --accent-soft: #ffe0ef;
    --sky: #bfe3ff;            /* powder blue accent */
    --sky-text: #2f5f86;
    --warn: #ffd89e;
    --warn-text: #7a4b00;
    --danger: #ff7a9c;
    --ok: #b8f0d4;
    --ok-text: #1f6a47;
    --panel-bg: #ffffff;
    --card-pink: #fff0f7;
    --shadow: 6px 6px 0 #000;
    --shadow-sm: 3px 3px 0 #000;
    --font-body: 'Nunito', 'Segoe UI Variable', 'Segoe UI', ui-rounded, system-ui, -apple-system, sans-serif;
    --font-head: 'Fredoka', 'Nunito', ui-rounded, 'Segoe UI', system-ui, sans-serif;
    --font-mono: 'JetBrains Mono', ui-monospace, 'Cascadia Mono', Consolas, monospace;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 2rem; font-family: var(--font-body); font-size: 16px; font-weight: 600;
    color: var(--text); line-height: 1.55; -webkit-font-smoothing: antialiased;
    background-color: var(--bg2); background-image: radial-gradient(#ffb3d6 1.6px, transparent 1.6px); background-size: 24px 24px;
    min-height: 100vh;
  }
  /* Layout Grid with Collapsible Sidebar */
  .layout {
    display: grid;
    grid-template-columns: 260px 1fr;
    gap: 2.5rem;
    max-width: 1240px;
    margin: 0 auto;
    align-items: start;
    transition: grid-template-columns 0.3s cubic-bezier(0.2, 0.9, 0.3, 1);
  }
  .layout.sidebar-collapsed {
    grid-template-columns: 1fr;
  }
  .layout.sidebar-collapsed .sidebar {
    display: none;
  }

  /* Top App Navigation Bar */
  .top-app-bar {
    max-width: 1240px;
    margin: 0 auto 1.25rem auto;
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 14px;
    padding: 0.7rem 1.1rem;
    box-shadow: var(--shadow);
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 0.8rem;
    position: sticky;
    top: 0.75rem;
    z-index: 1000;
  }
  .top-app-left {
    display: flex;
    align-items: center;
    gap: 0.8rem;
  }
  .top-logo {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    font-family: var(--font-head);
    font-size: 1.25rem;
    font-weight: 700;
    letter-spacing: 0.2px;
  }
  .top-logo span {
    color: #000;
    background: #ff8fc2;
    border: 2px solid #000;
    width: 1.9rem;
    height: 1.9rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 6px;
    font-size: 1.1rem;
    box-shadow: 2px 2px 0 #000;
  }
  .menu-toggle-btn {
    background: #ff8fc2;
    color: #000;
    border: 3px solid #000;
    border-radius: 10px;
    padding: 0.45rem 0.85rem;
    font-family: var(--font-body);
    font-size: 0.92rem;
    font-weight: 800;
    cursor: pointer;
    box-shadow: var(--shadow-sm);
    display: inline-flex;
    align-items: center;
    gap: 0.45rem;
    transition: all 0.15s ease;
  }
  .menu-toggle-btn:hover {
    background: #ff74b4;
    transform: translate(2px, 2px);
    box-shadow: 1px 1px 0 #000;
  }
  .menu-toggle-btn:active {
    transform: translate(3px, 3px);
    box-shadow: none;
  }
  .top-quick-select {
    display: flex;
    align-items: center;
    gap: 0.6rem;
  }
  .nav-select-dropdown {
    background: #fff;
    border: 3px solid #000;
    border-radius: 10px;
    padding: 0.45rem 0.75rem;
    font-family: var(--font-body);
    font-size: 0.9rem;
    font-weight: 800;
    color: var(--text);
    box-shadow: var(--shadow-sm);
    cursor: pointer;
    outline: none;
  }
  .nav-select-dropdown:focus {
    box-shadow: 4px 4px 0 #000;
    border-color: #ff74b4;
  }

  /* Sliding Horizontal Strip Carousel (Hidable) */
  .top-nav-strip-wrapper {
    max-width: 1240px;
    margin: 0 auto 1.5rem auto;
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 14px;
    padding: 0.65rem 0.9rem;
    box-shadow: var(--shadow);
    transition: all 0.3s ease;
  }
  .top-nav-strip-wrapper.hidden {
    display: none;
  }
  .strip-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.45rem;
    padding: 0 0.2rem;
  }
  .strip-title {
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-weight: 800;
    color: var(--text-muted);
    display: flex;
    align-items: center;
    gap: 0.4rem;
  }
  .strip-scroll-controls {
    display: flex;
    gap: 0.35rem;
  }
  .strip-arrow-btn {
    background: #fff;
    border: 2px solid #000;
    border-radius: 6px;
    width: 28px;
    height: 28px;
    font-weight: 800;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    box-shadow: 2px 2px 0 #000;
    transition: all 0.1s;
  }
  .strip-arrow-btn:hover {
    background: var(--card-pink);
    transform: translate(1px, 1px);
    box-shadow: 1px 1px 0 #000;
  }
  .top-nav-strip {
    display: flex;
    align-items: center;
    gap: 0.65rem;
    overflow-x: auto;
    padding: 0.3rem 0.2rem 0.5rem 0.2rem;
    scrollbar-width: thin;
    scrollbar-color: #ff8fc2 #ffe8f3;
    scroll-behavior: smooth;
    -webkit-overflow-scrolling: touch;
  }
  .top-nav-strip::-webkit-scrollbar {
    height: 6px;
  }
  .top-nav-strip::-webkit-scrollbar-track {
    background: #ffe8f3;
    border-radius: 4px;
    border: 1px solid #000;
  }
  .top-nav-strip::-webkit-scrollbar-thumb {
    background: #ff8fc2;
    border-radius: 4px;
    border: 1px solid #000;
  }
  .top-pill-btn {
    flex-shrink: 0;
    background: #fff;
    border: 3px solid #000;
    border-radius: 10px;
    padding: 0.55rem 1rem;
    font-family: var(--font-body);
    font-size: 0.92rem;
    font-weight: 800;
    color: var(--text);
    cursor: pointer;
    box-shadow: var(--shadow-sm);
    transition: all 0.15s ease;
    white-space: nowrap;
    display: inline-flex;
    align-items: center;
    gap: 0.45rem;
  }
  .top-pill-btn:hover {
    background: var(--card-pink);
    transform: translate(2px, 2px);
    box-shadow: 1px 1px 0 #000;
  }
  .top-pill-btn.active {
    background: #ff8fc2;
    color: #000;
    transform: translate(2px, 2px);
    box-shadow: 1px 1px 0 #000;
  }

  /* Sidebar Component */
  .sidebar {
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 16px;
    padding: 1.25rem;
    box-shadow: var(--shadow);
    transition: transform 0.3s cubic-bezier(0.2, 0.9, 0.3, 1), opacity 0.25s ease;
  }
  .sidebar-header-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
  }
  .sidebar h1 {
    font-family: var(--font-head);
    font-size: 1.45rem;
    font-weight: 700;
    margin: 0;
    display: flex;
    align-items: center;
    gap: 0.6rem;
    letter-spacing: 0.2px;
  }
  .sidebar h1 span {
    color: #000;
    background: #ff8fc2;
    border: 3px solid #000;
    width: 2.1rem;
    height: 2.1rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 8px;
    font-size: 1.2rem;
    box-shadow: 3px 3px 0 #000;
  }
  .sidebar-close-btn {
    background: #fff;
    border: 2px solid #000;
    border-radius: 6px;
    width: 28px;
    height: 28px;
    cursor: pointer;
    font-weight: 800;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    box-shadow: 2px 2px 0 #000;
    transition: all 0.1s;
    font-size: 1.1rem;
  }
  .sidebar-close-btn:hover {
    background: var(--danger);
    color: #fff;
    transform: translate(1px, 1px);
    box-shadow: 1px 1px 0 #000;
  }

  .clock-card { background: var(--panel-bg); border: 3px solid #000; border-radius: 12px; padding: 0.9rem 1rem; margin-bottom: 1.25rem; box-shadow: var(--shadow-sm); text-align: center; }
  .clock-time { font-family: var(--font-mono); font-size: 1.75rem; font-weight: 700; color: var(--accent-hover); letter-spacing: 1px; }
  .clock-date { font-size: 0.85rem; font-weight: 700; color: var(--text-muted); margin-top: 0.1rem; }
  .clock-sync { font-size: 0.72rem; font-weight: 700; margin-top: 0.4rem; color: var(--text-muted); }
  .clock-sync button { margin-left: 0.4rem; padding: 0.15rem 0.6rem; font-size: 0.72rem; border-radius: 8px; border: 3px solid #000; background: var(--accent-soft); color: var(--text); cursor: pointer; font-family: inherit; font-weight: 800; }
  .clock-sync button:hover { background: var(--accent); color: #fff; }

  nav { display: flex; flex-direction: column; gap: 0.6rem; }
  .nav-btn {
    text-align: left; background: var(--panel-bg); border: 3px solid #000;
    padding: 0.7rem 1rem; font-family: var(--font-body); font-size: 1rem; color: var(--text); cursor: pointer;
    border-radius: 10px; font-weight: 800; box-shadow: var(--shadow-sm); transition: all 0.15s;
    display: flex; align-items: center; gap: 0.5rem;
  }
  .nav-btn:hover { transform: translate(2px, 2px); box-shadow: 1px 1px 0 #000; background: var(--card-pink); }
  .nav-btn.active { background: #ff8fc2; color: #000; border-color: #000; transform: translate(2px, 2px); box-shadow: 1px 1px 0 #000; }

  /* Drawer Backdrop Overlay for Mobile */
  .drawer-backdrop {
    position: fixed;
    inset: 0;
    background: rgba(45, 20, 35, 0.45);
    backdrop-filter: blur(4px);
    z-index: 2400;
    display: none;
    opacity: 0;
    transition: opacity 0.25s ease;
  }
  .drawer-backdrop.show {
    display: block;
    opacity: 1;
  }

  /* Responsive Rules: Under 900px, Sidebar transforms into Slide-in Drawer */
  @media (max-width: 900px) {
    .layout { grid-template-columns: 1fr; gap: 1.5rem; }
    body { padding: 1rem 0.8rem; }
    .sidebar {
      position: fixed;
      top: 0;
      left: 0;
      bottom: 0;
      width: 290px;
      z-index: 2500;
      border-radius: 0 16px 16px 0;
      border-left: none;
      box-shadow: 8px 0 0 #000;
      background: var(--bg1);
      overflow-y: auto;
      transform: translateX(-105%);
    }
    .sidebar.mobile-drawer-open {
      transform: translateX(0);
    }
  }

  .section { display: none; }
  .section.active { display: block; animation: fade 0.25s ease; }
  @keyframes fade { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: none; } }

  h2 { font-family: var(--font-head); font-size: 1.9rem; font-weight: 700; margin: 0 0 1.25rem 0; padding-bottom: 0.5rem; border-bottom: 4px solid #000; }
  h3 { font-family: var(--font-head); font-size: 1.05rem; font-weight: 600; letter-spacing: 0.08em; text-transform: uppercase; color: #1a1a1a; background: #ffd1e6; display: inline-block; padding: 0.2rem 0.7rem; border: 3px solid #000; box-shadow: 3px 3px 0 #000; margin: 2.2rem 0 1rem 0; }
  h4 { font-family: var(--font-head); font-weight: 600; }

  .panel-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 1.25rem; }
  .panel { background: var(--panel-bg); padding: 1.4rem; border: 3px solid #000; border-radius: 12px; box-shadow: var(--shadow); }
  .panel-grid .panel:nth-child(2) { background: var(--card-pink); }

  .data-list { list-style: none; padding: 0; margin: 0; }
  .data-list li { display: flex; justify-content: space-between; align-items: center; padding: 0.7rem 0; border-bottom: 2px dashed #000; font-size: 0.95rem; font-weight: 700; }
  .data-list li:last-child { border-bottom: none; padding-bottom: 0; }
  .data-label { color: var(--text-muted); }
  .data-value { font-weight: 700; font-family: var(--font-mono); font-size: 1rem; }

  .tag { box-shadow: 2px 2px 0 #000; display: inline-block; padding: 0.22rem 0.8rem; border-radius: 8px; font-size: 0.8rem; font-weight: 800; border: 3px solid #000; background: #fff; color: var(--text); }
  .tag.active { background: var(--ok); border-color: #000; color: var(--ok-text); }
  .tag.alert { background: var(--danger); border-color: #000; color: #fff; }
  .tag.accent { background: var(--sky); border-color: #000; color: var(--sky-text); }
  .tag.warn { background: var(--warn); border-color: #000; color: var(--warn-text); }

  .control-group { margin-bottom: 1.25rem; }
  .control-group label { display: block; font-size: 0.95rem; font-weight: 800; margin-bottom: 0.45rem; }
  input[type="text"], input[type="number"], input[type="time"] {
    width: 100%; max-width: 300px; padding: 0.7rem 0.9rem; border: 3px solid #000;
    border-radius: 10px; font-size: 1rem; font-weight: 700; outline: none; font-family: inherit; color: var(--text); background: #fff;
  }
  input:focus { border-color: var(--accent); box-shadow: 4px 4px 0 #000; background: var(--bg1); }

  button.primary, button.accent, button.warn, button.danger {
    border: 3px solid #000; padding: 0.7rem 1.2rem; font-family: var(--font-body);
    font-size: 0.95rem; font-weight: 800; border-radius: 10px; cursor: pointer; transition: all 0.15s; box-shadow: var(--shadow-sm);
  }
  button.primary { background: #fff; color: var(--text); border-color: var(--border); }
  button.primary:hover { background: var(--card-pink); transform: translate(2px, 2px); box-shadow: 1px 1px 0 #000; }
  button.accent { background: #ff8fc2; color: #fff; }
  button.accent:hover { background: #ff74b4; transform: translate(2px, 2px); box-shadow: 1px 1px 0 #000; }
  button.warn { background: var(--warn); color: var(--warn-text); }
  button.warn:hover { transform: translate(2px, 2px); box-shadow: 1px 1px 0 #000; filter: brightness(0.97); }
  button.danger { background: var(--danger); color: #fff; }

  .dpad { display: grid; grid-template-columns: repeat(3, 90px); grid-template-rows: repeat(3, 60px); gap: 0.6rem; justify-content: center; margin: 1rem 0; }
  .dpad button {
    font-family: var(--font-body); font-weight: 800; border: 3px solid #000; border-radius: 10px; cursor: pointer;
    box-shadow: var(--shadow-sm); background: #fff; color: var(--text); font-size: 0.9rem; transition: all 0.12s;
  }
  .dpad button:hover { background: var(--card-pink); }
  .dpad button:active { transform: scale(0.95); background: var(--accent-soft); }
  .dpad button.stop-btn { background: var(--danger); color: #fff; border-color: #000; }

  canvas { width: 100%; max-height: 250px; margin-top: 1rem; }
  .log-viewer { font-family: var(--font-mono); font-size: 0.85rem; font-weight: 600; color: var(--text); background: var(--card-pink); padding: 1rem; border: 3px solid #000; border-radius: 10px; height: 150px; overflow-y: auto; }

  /* ---- Toasts & modal alerts ---- */
  .toast-wrap { position: fixed; top: 1rem; right: 1rem; display: flex; flex-direction: column; gap: 0.6rem; z-index: 1000; max-width: 340px; }
  .toast { background: #fff; border: 3px solid #000; border-left-width: 8px; border-radius: 10px; padding: 0.7rem 0.9rem; box-shadow: var(--shadow); animation: slidein 0.25s ease; font-size: 0.9rem; cursor: pointer; }
  .toast b { display: block; font-family: var(--font-head); font-weight: 600; font-size: 0.95rem; }
  .toast.critical { border-left-color: var(--danger); background: #fff0f4; }
  .toast.warn { border-left-color: #f5b85a; background: #fff8ec; }
  .toast.info { border-left-color: #8cc8f5; }
  .toast.ok { border-left-color: #6fd7a4; background: #f3fff9; }
  @keyframes slidein { from { opacity: 0; transform: translateX(30px); } to { opacity: 1; transform: none; } }

  .modal-overlay { position: fixed; inset: 0; background: rgba(91, 58, 87, 0.35); backdrop-filter: blur(3px); display: none; align-items: center; justify-content: center; z-index: 2000; padding: 1rem; }
  .modal-overlay.show { display: flex; }
  .modal { background: #fff; border: 3px solid var(--border); border-radius: 16px; padding: 1.6rem 1.8rem; max-width: 440px; width: 100%; text-align: center; box-shadow: 10px 10px 0 #000; animation: pop 0.2s ease; }
  @keyframes pop { from { transform: scale(0.9); opacity: 0; } to { transform: none; opacity: 1; } }
  .modal-icon { width: 3.4rem; height: 3.4rem; border-radius: 50%; margin: 0 auto 0.8rem; display: flex; align-items: center; justify-content: center; font-family: var(--font-head); font-size: 1.8rem; font-weight: 700; color: #fff; background: #8cc8f5; }
  .modal.critical .modal-icon { background: var(--danger); animation: pulse 1s infinite; }
  .modal.warn .modal-icon { background: #f5b85a; }
  .modal.ok .modal-icon { background: #6fd7a4; }
  @keyframes pulse { 0%,100% { box-shadow: 0 0 0 0 rgba(255,122,156,0.6); } 50% { box-shadow: 0 0 0 14px rgba(255,122,156,0); } }
  .modal h3 { background: none; border: none; box-shadow: none; padding: 0; display: block; margin: 0 0 0.4rem 0; font-size: 1.3rem; letter-spacing: 0; text-transform: none; color: var(--text); }
  .modal p { margin: 0 0 1.2rem 0; color: var(--text-muted); font-weight: 700; }
  .modal-actions { display: flex; gap: 0.6rem; justify-content: center; flex-wrap: wrap; }

  /* ---- Calibration ---- */
  .sbar-row { display: grid; grid-template-columns: 34px 1fr 52px; gap: 0.6rem; align-items: center; margin: 0.35rem 0; font-size: 0.85rem; font-weight: 800; }
  .sbar-track { position: relative; height: 16px; background: var(--card-pink); border: 3px solid #000; border-radius: 8px; overflow: hidden; }
  .sbar-fill { height: 100%; width: 0; background: #ff8fc2; transition: width 0.25s; }
  .sbar-fill.black { background: #1a1a1a; }
  .sbar-fill.line { background: #8cc8f5; }
  .sbar-track i { position: absolute; top: -2px; bottom: -2px; width: 3px; }
  .sbar-track i.mk-b { background: #1a1a1a; }
  .sbar-track i.mk-l { background: #2f8fd8; }
  .legend { font-size: 0.78rem; color: var(--text-muted); margin-top: 0.6rem; }
  .legend .dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin: 0 4px 0 10px; vertical-align: middle; }
  .cal-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 1rem 1.25rem; }
  .cal-grid input { max-width: 100%; }
  .hint { font-size: 0.78rem; font-weight: 700; color: var(--text-muted); margin-top: 0.3rem; }
  .switch-row { display: flex; align-items: center; gap: 0.6rem; margin: 0.6rem 0; font-weight: 800; }
  .switch-row input[type="checkbox"] { width: 20px; height: 20px; accent-color: #ff74b4; }
  .alert-item { display: flex; gap: 0.6rem; padding: 0.5rem 0; border-bottom: 2px dashed #000; font-size: 0.88rem; }
  .alert-item:last-child { border-bottom: none; }
  .alert-time { font-family: var(--font-mono); color: var(--text-muted); font-size: 0.8rem; min-width: 64px; }
  .terminal-box {
    height: 320px;
    overflow-y: auto;
    font-family: var(--font-mono);
    font-size: 0.85rem;
    line-height: 1.45;
    background: #0f0e13;
    padding: 0.8rem;
    border: 3px solid #000;
    border-radius: 8px;
    white-space: pre-wrap;
    word-break: break-all;
    color: #ff9ec8;
    box-shadow: inset 2px 2px 0 #000;
  }
  #unoConsole { color: #a8f0ff; }

  /* ---- Neobrutalist Status Ribbon ---- */
  .status-ribbon {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    margin-bottom: 1.5rem;
    align-items: center;
  }
  .status-pill {
    background: var(--panel-bg);
    border: 3px solid #000;
    border-radius: 10px;
    padding: 0.5rem 0.9rem;
    box-shadow: var(--shadow-sm);
    display: inline-flex;
    align-items: center;
    gap: 0.5rem;
    font-size: 0.88rem;
    font-weight: 800;
  }
  .status-pill.active {
    background: var(--ok);
    color: var(--ok-text);
  }
  .status-pill.alert {
    background: var(--danger);
    color: #fff;
  }
  .status-pill.warn {
    background: var(--warn);
    color: var(--warn-text);
  }
  .status-dot-pulse {
    display: inline-block;
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: #008035;
    box-shadow: 0 0 0 0 rgba(0, 200, 83, 0.7);
    animation: pulseDot 1.4s infinite;
  }
  .status-dot-red {
    display: inline-block;
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: #ffffff;
    border: 2px solid #000;
  }
  @keyframes pulseDot {
    0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(0, 200, 83, 0.7); }
    70% { transform: scale(1); box-shadow: 0 0 0 8px rgba(0, 200, 83, 0); }
    100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(0, 200, 83, 0); }
  }
</style>
<script>
  let chart;
  function initChart() {
    Chart.defaults.font.family = 'Nunito, Segoe UI, system-ui, sans-serif';
    Chart.defaults.color = '#1a1a1a';
    Chart.defaults.font.weight = 'bold';
    const ctx = document.getElementById('disposalChart').getContext('2d');
    chart = new Chart(ctx, { type: 'bar', data: { labels: ['Today'], datasets: [{ label: 'Disposals', data: [0], backgroundColor: '#ff9ec8', borderColor: '#000000', borderWidth: 3, borderRadius: 4 }] }, options: { responsive: true, scales: { y: { beginAtZero: true, ticks: { stepSize: 1 } }, x: { grid: { display: false } } }, plugins: { legend: { display: false } } } });
  }

  function scanWifi() {
    notify('info', 'Scanning', 'Scanning available Wi-Fi networks...');
    const box = document.getElementById('wifiScanResults');
    const list = document.getElementById('wifiList');
    box.style.display = 'block';
    list.innerHTML = 'Scanning nearby networks...';
    fetch('/wifi_scan').then(r => r.json()).then(nets => {
      if (!nets || !nets.length) {
        list.innerHTML = '<div class="hint">No networks found.</div>';
        return;
      }
      list.innerHTML = nets.map(n => 
        '<div style="cursor:pointer; padding:0.3rem 0.5rem; border-bottom:1px dashed #000; display:flex; justify-content:space-between; align-items:center;" onclick="selectWifi(\'' + n.ssid.replace(/'/g, "\\'") + '\')">' +
        '<b>' + n.ssid + '</b> <span style="font-size:0.75rem; color:var(--text-muted);">' + (n.open ? 'OPEN' : 'SECURE') + ' (' + n.rssi + ' dBm)</span></div>'
      ).join('');
    }).catch(e => {
      list.innerHTML = '<div class="hint">Failed to scan.</div>';
    });
  }

  function selectWifi(ssid) {
    document.getElementById('wifiSsidInput').value = ssid;
    document.getElementById('wifiPassInput').focus();
  }

  function saveWifi() {
    const s = document.getElementById('wifiSsidInput').value.trim();
    const p = document.getElementById('wifiPassInput').value;
    const apP = document.getElementById('apPassInput') ? document.getElementById('apPassInput').value.trim() : '';
    if (!s) { alert('Please enter an SSID'); return; }
    if (apP && apP.length < 8) { alert('Hotspot password must be at least 8 characters (WPA2)'); return; }
    showModal({
      level: 'warn',
      title: 'Connect to ' + s + '?',
      msg: 'The robot will save these credentials and reboot to connect. If out of range, the fallback hotspot MedWaste-Master-AP will remain active with your known password.',
      actions: [{
        label: 'Reboot & Connect',
        cls: 'accent',
        fn: () => {
          let url = '/set_wifi?ssid=' + encodeURIComponent(s) + '&pass=' + encodeURIComponent(p);
          if (apP) url += '&ap_pass=' + encodeURIComponent(apP);
          fetch(url)
            .then(r => r.text()).then(t => {
              notify('ok', 'Rebooting', t, { modal: true });
            });
        }
      }]
    });
  }

  function saveApPassOnly() {
    const apP = document.getElementById('apPassInput') ? document.getElementById('apPassInput').value.trim() : '';
    if (!apP || apP.length < 8) { alert('Hotspot password must be at least 8 characters (WPA2 standard)'); return; }
    fetch('/set_ap_pass?pass=' + encodeURIComponent(apP))
      .then(r => r.text()).then(t => {
        notify('ok', 'Hotspot Password', t);
        updateData();
      });
  }

  function resetWifi() {
    showModal({
      level: 'critical',
      title: 'Reset Wi-Fi Settings?',
      msg: 'This will erase saved Wi-Fi credentials and keep the robot running in Access Point Hotspot mode (MedWaste-Master-AP).',
      actions: [{
        label: 'Reset Now',
        cls: 'danger',
        fn: () => {
          fetch('/reset_wifi').then(r => r.text()).then(t => {
            notify('warn', 'Hotspot Mode', t, { modal: true });
          });
        }
      }]
    });
  }

  function showSection(id) {
    document.querySelectorAll('.section').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-btn').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.top-pill-btn').forEach(el => el.classList.remove('active'));
    const targetSection = document.getElementById(id);
    if (targetSection) targetSection.classList.add('active');
    document.querySelectorAll(`[data-target="${id}"]`).forEach(el => el.classList.add('active'));
    const dd = document.getElementById('navSelectDropdown');
    if (dd) dd.value = id;
    
    // Smoothly scroll active top pill into view if needed
    const activePill = document.querySelector(`.top-pill-btn[data-target="${id}"]`);
    if (activePill) {
      activePill.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'center' });
    }

    // Auto-close slide drawer on mobile
    if (window.innerWidth <= 900) {
      closeSidebarDrawer();
    }
  }

  function toggleSidebar() {
    const layout = document.querySelector('.layout');
    const sidebar = document.getElementById('sidebarPanel');
    if (window.innerWidth <= 900) {
      const isOpen = sidebar && sidebar.classList.contains('mobile-drawer-open');
      if (isOpen) { closeSidebarDrawer(); } else { openSidebarDrawer(); }
    } else {
      if (layout) {
        layout.classList.toggle('sidebar-collapsed');
        const isCollapsed = layout.classList.contains('sidebar-collapsed');
        localStorage.setItem('medwaste_sidebar_collapsed', isCollapsed ? '1' : '0');
        updateSidebarToggleBtnText();
      }
    }
  }

  function openSidebarDrawer() {
    const sidebar = document.getElementById('sidebarPanel');
    const overlay = document.getElementById('drawerBackdrop');
    if (sidebar) sidebar.classList.add('mobile-drawer-open');
    if (overlay) overlay.classList.add('show');
    updateSidebarToggleBtnText();
  }

  function closeSidebarDrawer() {
    const sidebar = document.getElementById('sidebarPanel');
    const overlay = document.getElementById('drawerBackdrop');
    if (sidebar) sidebar.classList.remove('mobile-drawer-open');
    if (overlay) overlay.classList.remove('show');
    updateSidebarToggleBtnText();
  }

  function updateSidebarToggleBtnText() {
    const btn = document.getElementById('sidebarToggleBtn');
    if (!btn) return;
    const layout = document.querySelector('.layout');
    const sidebar = document.getElementById('sidebarPanel');
    const isMobile = window.innerWidth <= 900;
    if (isMobile) {
      const isOpen = sidebar && sidebar.classList.contains('mobile-drawer-open');
      btn.innerHTML = isOpen ? '<span>✕</span> Close Menu' : '<span>☰</span> Menu';
    } else {
      const isCollapsed = layout && layout.classList.contains('sidebar-collapsed');
      btn.innerHTML = isCollapsed ? '<span>☰</span> Show Menu' : '<span>⇤</span> Hide Menu';
    }
  }

  function toggleTopNav() {
    const navBar = document.getElementById('topNavStripWrapper');
    if (!navBar) return;
    navBar.classList.toggle('hidden');
    const isHidden = navBar.classList.contains('hidden');
    localStorage.setItem('medwaste_topnav_hidden', isHidden ? '1' : '0');
    const tBtn = document.getElementById('topNavToggleBtn');
    if (tBtn) {
      tBtn.innerHTML = isHidden ? '<span>↕</span> Show Bar' : '<span>✕</span> Hide Bar';
    }
  }

  function scrollNavStrip(dir) {
    const strip = document.getElementById('topNavStrip');
    if (strip) {
      strip.scrollBy({ left: dir * 180, behavior: 'smooth' });
    }
  }

  function initNavPreferences() {
    if (localStorage.getItem('medwaste_sidebar_collapsed') === '1' && window.innerWidth > 900) {
      const layout = document.querySelector('.layout');
      if (layout) layout.classList.add('sidebar-collapsed');
    }
    if (localStorage.getItem('medwaste_topnav_hidden') === '1') {
      const navBar = document.getElementById('topNavStripWrapper');
      if (navBar) navBar.classList.add('hidden');
      const tBtn = document.getElementById('topNavToggleBtn');
      if (tBtn) tBtn.innerHTML = '<span>↕</span> Show Bar';
    }
    updateSidebarToggleBtnText();
    window.addEventListener('resize', updateSidebarToggleBtnText);
  }

  function getStatusTag(state, obstacle) {
    if (state === 'MANUAL') return `<span class="tag warn">Manual Override</span>`;
    if (obstacle === 'Yes') return `<span class="tag alert">Obstacle</span>`;
    if (state === 'MOVING' || state === 'RETURNING' || state === 'SPINNING' || state === 'DUMPING') return `<span class="tag accent">${state}</span>`;
    if (state === 'IDLE') return `<span class="tag active">Idle</span>`;
    return `<span class="tag">Offline</span>`;
  }

  function triggerDisposal() { fetch('/dispose').then(r => r.text()).then(t => notify('info', 'Disposal', t)); }
  function sendManualCmd(cmd) {
    fetch('/manual?cmd=' + encodeURIComponent(cmd)).then(r => r.text()).then(t => {
      updateData();
    });
  }
  function setSchedule() {
    const t = document.getElementById('schedTime').value;
    if(t) fetch('/set_schedule?time=' + t).then(r => r.text()).then(t => notify('ok', 'Saved', t));
  }
  function updateSettings() {
    const s = document.getElementById('speedInp').value;
    const o = document.getElementById('obsInp').value;
    const t = document.getElementById('trashInp').value;
    fetch(`/set_settings?s=${s}&o=${o}&t=${t}`).then(r => r.text()).then(t => notify('ok', 'Saved', t));
  }
  function updateNames() {
    const m = document.getElementById('nameInpM').value;
    const s = document.getElementById('nameInpS').value;
    fetch(`/set_names?m=${encodeURIComponent(m)}&s=${encodeURIComponent(s)}`).then(r => r.text()).then(t => notify('ok', 'Saved', t));
  }

  function updateData() {
    fetch('/data').then(r => r.json()).then(data => {
      connectionOk();
      document.getElementById('m_hdr').innerText = data.masterName;
      if (document.getElementById('s_hdr')) document.getElementById('s_hdr').innerText = data.slaveName;
      if (data.wifi) {
        document.getElementById('curWifiSsid').innerText = data.wifi.ssid || '-';
        document.getElementById('curWifiIp').innerText = data.wifi.ip || '-';
        document.getElementById('wifiStatusBadge').innerHTML = data.wifi.staConnected 
          ? '<span class="tag active">CONNECTED TO ' + (data.wifi.ssid || 'NETWORK') + '</span>' 
          : '<span class="tag warn">HOTSPOT MODE (AP)</span>';
        if (document.getElementById('pillApSsid')) document.getElementById('pillApSsid').innerText = data.wifi.apSsid || 'MedWaste-Master-AP';
        if (document.getElementById('pillApPass')) document.getElementById('pillApPass').innerText = 'pass: ' + (data.wifi.apPass || 'medwaste123');
        if (document.getElementById('apPassInput') && !document.getElementById('apPassInput').matches(':focus')) {
          document.getElementById('apPassInput').value = data.wifi.apPass || 'medwaste123';
        }
      }

      // Slave Connection Status Pill Update
      const slavePill = document.getElementById('slaveStatusPill');
      const slaveTxt = document.getElementById('slaveStatusTxt');
      if (slavePill && slaveTxt) {
        let connectedCount = 0;
        let firstOnline = null;
        if (data.swarm && data.swarm.length > 0) {
          data.swarm.forEach(s => {
            if (s.online) {
              connectedCount++;
              if (!firstOnline) firstOnline = s;
            }
          });
        }
        if (connectedCount > 0 && firstOnline) {
          slavePill.className = 'status-pill active';
          slaveTxt.innerHTML = '<span class="status-dot-pulse"></span> <b>SLAVE CONNECTED:</b> ' + firstOnline.name + ' (' + firstOnline.id + (firstOnline.ip ? ' @ ' + firstOnline.ip : '') + ')';
        } else {
          slavePill.className = 'status-pill alert';
          slaveTxt.innerHTML = '<span class="status-dot-red"></span> <b>SLAVE ROBOT: DISCONNECTED</b> (Waiting for slave heartbeat...)';
        }
      }

      document.getElementById('modeBadge').innerHTML = data.manualMode
        ? `<span class="tag warn">MANUAL OVERRIDE</span>`
        : `<span class="tag active">AUTO MODE</span>`;

      const m_csv = (data.masterUnoStatus && data.masterUnoStatus !== 'Offline') ? data.masterUnoStatus.split(',') : null;
      const s_csv = (data.slaveUnoStatus && data.slaveUnoStatus !== 'Offline') ? data.slaveUnoStatus.split(',') : null;

      // Render Dynamic Swarm Fleet
      renderSwarm(data);

      let alerts = [];
      if (m_csv && m_csv.length > 7 && m_csv[7] === '1') alerts.push(`${data.masterName}: Obstacle in Path!`);
      if (data.masterTrashLevel === 'Full') alerts.push(`${data.masterName}: Trash is Full!`);
      
      if (data.swarm && data.swarm.length > 0) {
        data.swarm.forEach(slv => {
          if (slv.trashLevel === 'Full') alerts.push(`${slv.name}: Trash is Full!`);
        });
      }
      
      const abox = document.getElementById('globalAlert');
      if (alerts.length > 0) {
        abox.innerHTML = "<strong>ALERT:</strong> " + alerts.join(" | ");
        abox.style.display = 'block';
      } else {
        abox.style.display = 'none';
      }
      
      document.getElementById('history').innerHTML = data.history.replace(/\n/g, '<br>') || 'No logs yet.';
      document.getElementById('schedInfo').innerText = data.schedTime;
      chart.data.datasets[0].data[0] = data.disposalCount;
      chart.update();

      // Re-anchor the live clock to the ESP32 web server's own time
      srvLocalEpoch = data.localEpoch;
      srvFetchedAt = performance.now();
      srvSynced = data.timeSynced;
      if (!srvSynced && !autoSyncTried) { autoSyncTried = true; syncTime(); }
      renderClock();
      renderCalib(data, m_csv, s_csv);
      runAlerts(data, m_csv, s_csv);
    }).catch(connectionLost);
  }

  // --- Dynamic Swarm Fleet Telemetry Renderer ---
  function renderSwarm(data) {
    const grid = document.getElementById('swarmTelemetryGrid');
    if (!grid) return;
    const m_csv = (data.masterUnoStatus && data.masterUnoStatus !== 'Offline') ? data.masterUnoStatus.split(',') : null;
    let html = `
      <div class="panel">
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.8rem;">
          <h4 id="m_hdr" style="margin:0; font-size:1.2rem; font-weight:800;">${data.masterName} <span class="tag accent" style="font-size:0.7rem;">MASTER</span></h4>
          <span class="tag active"><span style="color:#008035; font-size:1.1rem; vertical-align:middle;">●</span> ONLINE</span>
        </div>
        <ul class="data-list">
          <li><span class="data-label">Role & Unit</span> <span class="data-value">Swarm Master</span></li>
          <li><span class="data-label">Operational State</span> <span class="data-value">${getStatusTag(m_csv ? m_csv[8] : 'Offline', m_csv ? (m_csv[7]==='1'?'Yes':'No') : 'No')}</span></li>
          <li><span class="data-label">Motor Speed</span> <span class="data-value">${m_csv ? m_csv[0] : '-'}</span></li>
          <li><span class="data-label">Sensor Array</span> <span class="data-value">${m_csv ? `[${m_csv[1]},${m_csv[2]},${m_csv[3]},${m_csv[4]},${m_csv[5]}]` : '-'}</span></li>
          <li><span class="data-label">Front Ultrasonic</span> <span class="data-value">${(m_csv && m_csv.length > 9) ? (m_csv[9] + ' cm') : '-'}</span></li>
          <li><span class="data-label">Capacity</span> <span class="data-value"><span class="${data.masterTrashLevel==='Full'?'tag alert':data.masterTrashLevel==='Halfway'?'tag active':'tag'}">${data.masterTrashLevel} (${data.masterTrashCm}cm)</span></span></li>
          <li><span class="data-label">Lid & Proximity</span> <span class="data-value">${data.masterTrashLevel==='Full' ? '<span class="tag alert" style="font-size:0.75rem;">LOCKED (BIN FULL)</span>' : data.disposing ? '<span class="tag warn" style="font-size:0.75rem;">LOCKED (DISPOSING)</span>' : data.lidOpen ? '<span class="tag active" style="font-size:0.75rem;">OPEN (PROXIMITY)</span>' : '<span class="tag" style="font-size:0.75rem;">CLOSED (READY)</span>'}</span></li>
        </ul>
      </div>`;

    if (data.swarm && data.swarm.length > 0) {
      data.swarm.forEach((slv) => {
        const s_csv = (slv.status && slv.status !== 'Offline' && slv.status !== 'IDLE') ? slv.status.split(',') : null;
        const onlineTag = slv.online 
          ? '<span class="tag active" style="font-size:0.85rem;"><span style="color:#008035; font-size:1.1rem; vertical-align:middle;">●</span> CONNECTED</span>' 
          : '<span class="tag alert" style="font-size:0.85rem;">DISCONNECTED</span>';
        html += `
          <div class="panel" style="background:var(--card-pink);">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.8rem;">
              <h4 style="margin:0; font-size:1.2rem; font-weight:800;">${slv.name} <span class="tag" style="font-size:0.7rem;">${slv.id}</span></h4>
              ${onlineTag}
            </div>
            <ul class="data-list">
              <li><span class="data-label">Connection Status</span> <span class="data-value">${slv.online ? '<span style="color:#008035; font-weight:800;">Connected (' + (slv.secAgo !== undefined ? slv.secAgo : 0) + 's ago)</span>' : '<span style="color:#d32f2f; font-weight:800;">Lost (' + (slv.secAgo !== undefined ? slv.secAgo : '?') + 's ago)</span>'}</span></li>
              <li><span class="data-label">Operational State</span> <span class="data-value">${getStatusTag(s_csv ? s_csv[8] : (slv.status || 'IDLE'), s_csv ? (s_csv[7]==='1'?'Yes':'No') : 'No')}</span></li>
              <li><span class="data-label">Motor Speed</span> <span class="data-value">${s_csv ? s_csv[0] : '-'}</span></li>
              <li><span class="data-label">Sensor Array</span> <span class="data-value">${s_csv ? `[${s_csv[1]},${s_csv[2]},${s_csv[3]},${s_csv[4]},${s_csv[5]}]` : '-'}</span></li>
              <li><span class="data-label">Capacity</span> <span class="data-value"><span class="${slv.trashLevel==='Full'?'tag alert':slv.trashLevel==='Halfway'?'tag active':'tag'}">${slv.trashLevel} (${slv.trashCm}cm)</span></span></li>
              <li><span class="data-label">Lid & Proximity</span> <span class="data-value">${slv.trashLevel==='Full' ? '<span class="tag alert" style="font-size:0.75rem;">LOCKED (BIN FULL)</span>' : data.disposing ? '<span class="tag warn" style="font-size:0.75rem;">LOCKED (DISPOSING)</span>' : '<span class="tag" style="font-size:0.75rem;">CLOSED (READY)</span>'}</span></li>
              <li><span class="data-label">IP Address</span> <span class="data-value" style="font-size:0.85rem;">${slv.ip || '-'}</span></li>
            </ul>
          </div>`;
      });
      if (document.getElementById('swarmCountBadge')) document.getElementById('swarmCountBadge').innerText = String(1 + data.swarm.length);
    } else {
      html += `
        <div class="panel" style="background:var(--card-pink); text-align:center; padding:1.5rem;">
          <div style="margin-bottom:0.6rem;"><span class="tag alert" style="font-size:0.85rem; font-weight:800;">SLAVE ROBOT: NOT CONNECTED</span></div>
          <h4 style="margin:0 0 0.5rem 0; font-size:1.15rem; font-weight:800;">Waiting for Slave Robot Connection</h4>
          <p class="hint" style="margin:0 0 0.8rem 0; font-size:0.9rem;">Power on the Slave ESP32-C3 unit. Once connected to Wi-Fi, it will automatically register with this Master and appear here live.</p>
          <div style="font-size:0.85rem; background:#fff; border:2px dashed #000; border-radius:8px; padding:0.6rem; display:inline-block; font-family:var(--font-mono);">
            Master Endpoint: <b>http://${data.wifi && data.wifi.ip ? data.wifi.ip : '192.168.4.1'}/slave_update</b><br>or <b>http://medwaste.local/slave_update</b>
          </div>
        </div>`;
      if (document.getElementById('swarmCountBadge')) document.getElementById('swarmCountBadge').innerText = '1';
    }
    grid.innerHTML = html;
  }

  // --- Real-time Dual Serial Monitors Polling ---
  function updateTerminals() {
    fetch('/terminal_data').then(r => r.json()).then(data => {
      const eBox = document.getElementById('espConsole');
      const uBox = document.getElementById('unoConsole');
      const auto = document.getElementById('termAutoScroll') ? document.getElementById('termAutoScroll').checked : true;
      if (eBox && data.esp) {
        eBox.innerText = data.esp.join('\n') || 'ESP32 terminal idle.';
        if (auto) eBox.scrollTop = eBox.scrollHeight;
      }
      if (uBox && data.uno) {
        uBox.innerText = data.uno.join('\n') || 'Uno UART stream idle.';
        if (auto) uBox.scrollTop = uBox.scrollHeight;
      }
    }).catch(e => {});
  }

  function clearTerminals() {
    const e = document.getElementById('espConsole');
    const u = document.getElementById('unoConsole');
    if (e) e.innerText = '';
    if (u) u.innerText = '';
  }

  function sendDirectUnoCmd() {
    const inp = document.getElementById('unoCmdInput');
    const cmd = inp.value.trim();
    if (!cmd) return;
    fetch('/send_uno?cmd=' + encodeURIComponent(cmd)).then(r => r.text()).then(t => {
      notify('ok', 'Sent to Uno', t);
      inp.value = '';
      setTimeout(updateTerminals, 300);
    });
  }

  function sendQuickUno(cmd) {
    fetch('/send_uno?cmd=' + encodeURIComponent(cmd)).then(r => r.text()).then(t => {
      notify('info', 'Command Sent', cmd);
      setTimeout(updateTerminals, 300);
    });
  }

  function clearHistory() {
    showModal({
      level: 'warn',
      title: 'Clear Stored History?',
      msg: 'This will erase the disposal log from internal flash memory and reset the disposal count to 0.',
      actions: [{
        label: 'Clear Flash History',
        cls: 'danger',
        fn: () => {
          fetch('/clear_history').then(r => r.text()).then(t => {
            notify('ok', 'History Cleared', t);
            updateData();
          });
        }
      }]
    });
  }

  // --- Real-time clock (no RTC module): the ESP32 web server keeps time, the browser seeds it ---
  let srvLocalEpoch = 0, srvFetchedAt = 0, srvSynced = false, autoSyncTried = false;
  const DAYS = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
  const MONTHS = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  const pad = n => String(n).padStart(2, '0');

  function renderClock() {
    const t = document.getElementById('clockTime');
    if (!srvSynced) {
      t.innerText = '--:--:--';
      document.getElementById('clockDate').innerText = 'Time not set';
      document.getElementById('clockSyncTxt').innerText = 'Not synced';
      return;
    }
    const secs = srvLocalEpoch + Math.floor((performance.now() - srvFetchedAt) / 1000);
    const d = new Date(secs * 1000); // read with UTC getters: epoch already shifted to local time
    t.innerText = pad(d.getUTCHours()) + ':' + pad(d.getUTCMinutes()) + ':' + pad(d.getUTCSeconds());
    document.getElementById('clockDate').innerText = DAYS[d.getUTCDay()] + ', ' + MONTHS[d.getUTCMonth()] + ' ' + d.getUTCDate() + ', ' + d.getUTCFullYear();
    document.getElementById('clockSyncTxt').innerText = 'Server clock';
    checkSchedule();
  }

  function syncTime() {
    const now = new Date();
    fetch('/set_time?epoch=' + Math.floor(now.getTime() / 1000) + '&tz=' + (-now.getTimezoneOffset()))
      .then(r => r.text()).then(() => updateData());
  }

  window.onload = function() {
    initChart(); buildBars('cal_m'); buildBars('cal_s'); loadPrefsUI(); updateData();
    initNavPreferences();
    setInterval(updateData, 1500);
    setInterval(renderClock, 1000);
    setInterval(syncTime, 30 * 60 * 1000); // keep the server clock fresh every 30 min
    updateTerminals();
    setInterval(updateTerminals, 1200);
  }


  // ================= Alerts & notifications =================
  const alertPrefs = Object.assign({ sound: true, modal: true, browser: false, leadMin: 5 }, JSON.parse(localStorage.getItem('alertPrefs') || '{}'));
  const alertLog = [];
  const modalQueue = [];
  let modalOpen = false, audioCtx = null, lastData = null;
  const DISP_NAMES = ['Idle', 'Heading to disposal area', 'Tilting to dump', 'Holding tilt', 'Returning upright', 'Returning to base', 'Manual tilt down', 'Manual tilt up'];

  function nowStr() {
    if (!srvSynced) return new Date().toLocaleTimeString();
    const d = new Date((srvLocalEpoch + Math.floor((performance.now() - srvFetchedAt) / 1000)) * 1000);
    return pad(d.getUTCHours()) + ':' + pad(d.getUTCMinutes()) + ':' + pad(d.getUTCSeconds());
  }

  function beep(level) {
    if (!alertPrefs.sound) return;
    try {
      audioCtx = audioCtx || new (window.AudioContext || window.webkitAudioContext)();
      const o = audioCtx.createOscillator(), g = audioCtx.createGain();
      o.type = 'sine'; o.frequency.value = (level === 'critical') ? 520 : 880;
      g.gain.value = 0.08; o.connect(g); g.connect(audioCtx.destination);
      o.start(); o.stop(audioCtx.currentTime + (level === 'critical' ? 0.5 : 0.18));
    } catch (e) {}
  }

  function toast(level, title, msg) {
    const wrap = document.getElementById('toastWrap');
    const el = document.createElement('div');
    el.className = 'toast ' + level;
    el.innerHTML = '<b></b><span></span>';
    el.querySelector('b').innerText = title;
    el.querySelector('span').innerText = msg;
    el.onclick = () => el.remove();
    wrap.appendChild(el);
    setTimeout(() => el.remove(), level === 'critical' ? 12000 : 6000);
    while (wrap.children.length > 5) wrap.firstChild.remove();
  }

  function showModal(opts) { modalQueue.push(opts); if (!modalOpen) nextModal(); }
  function nextModal() {
    const o = modalQueue.shift();
    if (!o) { modalOpen = false; document.getElementById('modalOverlay').classList.remove('show'); return; }
    modalOpen = true;
    const box = document.getElementById('modalBox');
    box.className = 'modal ' + o.level;
    document.getElementById('modalIcon').innerText = o.level === 'ok' ? '\u2713' : (o.level === 'info' ? 'i' : '!');
    document.getElementById('modalTitle').innerText = o.title;
    document.getElementById('modalMsg').innerText = o.msg;
    const acts = document.getElementById('modalActions');
    acts.innerHTML = '';
    (o.actions || []).forEach(a => {
      const b = document.createElement('button');
      b.className = a.cls || 'primary'; b.innerText = a.label;
      b.onclick = () => { try { a.fn(); } catch (e) {} nextModal(); };
      acts.appendChild(b);
    });
    const d = document.createElement('button');
    d.className = 'accent'; d.innerText = (o.actions && o.actions.length) ? 'Dismiss' : 'OK';
    d.onclick = nextModal;
    acts.appendChild(d);
    document.getElementById('modalOverlay').classList.add('show');
  }

  function renderAlertLog() {
    const box = document.getElementById('alertHistory');
    if (!box) return;
    if (!alertLog.length) { box.innerHTML = '<div class="hint">No alerts yet.</div>'; return; }
    box.innerHTML = alertLog.map(a => '<div class="alert-item"><span class="alert-time">' + a.time + '</span><span class="tag ' +
      (a.level === 'critical' ? 'alert' : a.level === 'warn' ? 'warn' : a.level === 'ok' ? 'active' : 'accent') + '">' + a.level + '</span><span><b>' + a.title + '</b> &ndash; ' + a.msg + '</span></div>').join('');
  }

  // level: critical | warn | info | ok.  opts: { modal, push, actions }
  function notify(level, title, msg, opts) {
    opts = opts || {};
    alertLog.unshift({ level: level, title: title, msg: msg, time: nowStr() });
    if (alertLog.length > 50) alertLog.pop();
    renderAlertLog();
    toast(level, title, msg);
    beep(level);
    if (opts.modal && alertPrefs.modal) showModal({ level: level, title: title, msg: msg, actions: opts.actions });
    if (alertPrefs.browser && window.Notification && Notification.permission === 'granted' && (level === 'critical' || level === 'warn' || opts.push)) {
      try { new Notification(title, { body: msg }); } catch (e) {}
    }
  }

  function applyPrefs() {
    alertPrefs.modal = document.getElementById('prefModal').checked;
    alertPrefs.sound = document.getElementById('prefSound').checked;
    alertPrefs.leadMin = Math.max(1, parseInt(document.getElementById('prefLead').value) || 5);
    localStorage.setItem('alertPrefs', JSON.stringify(alertPrefs));
  }
  function loadPrefsUI() {
    document.getElementById('prefModal').checked = alertPrefs.modal;
    document.getElementById('prefSound').checked = alertPrefs.sound;
    document.getElementById('prefBrowser').checked = alertPrefs.browser && window.Notification && Notification.permission === 'granted';
    document.getElementById('prefLead').value = alertPrefs.leadMin;
    if (!window.Notification) document.getElementById('notifStatus').innerText = 'Browser notifications are not supported here. In-page alerts still work.';
  }
  function toggleBrowserNotif(cb) {
    const st = document.getElementById('notifStatus');
    if (!cb.checked) { alertPrefs.browser = false; localStorage.setItem('alertPrefs', JSON.stringify(alertPrefs)); st.innerText = ''; return; }
    if (!window.Notification) { cb.checked = false; st.innerText = 'Not supported by this browser.'; return; }
    Notification.requestPermission().then(p => {
      alertPrefs.browser = (p === 'granted');
      cb.checked = alertPrefs.browser;
      localStorage.setItem('alertPrefs', JSON.stringify(alertPrefs));
      st.innerText = alertPrefs.browser ? 'Enabled.' : 'Blocked. Note: browsers only allow notifications on https or localhost; in-page alerts, sound and pop-ups still work over plain http.';
    });
  }

  // --- connection state ---
  let connLost = false;
  function connectionLost() {
    if (connLost) return;
    connLost = true;
    notify('critical', 'Connection lost', 'Cannot reach the controller. Check power and WiFi.', { modal: true });
  }
  function connectionOk() {
    if (!connLost) return;
    connLost = false;
    notify('ok', 'Reconnected', 'Controller is back online.');
  }

  // --- event detection (runs on every telemetry update) ---
  const prev = {};
  function runAlerts(data, m, s) {
    lastData = data;
    const first = !prev.init;
    const obsRange = (data.cal && data.cal.obs) || 30;
    const units = [
      { k: 'm', name: data.masterName, csv: m, trash: data.masterTrashLevel, trashCm: data.masterTrashCm },
      { k: 's', name: data.slaveName,  csv: s, trash: data.slaveTrashLevel,  trashCm: data.slaveTrashCm }
    ];
    units.forEach(u => {
      const obs = !!(u.csv && u.csv[7] === '1');
      const dist = u.csv ? parseInt(u.csv[9]) : -1;
      const near = !obs && dist > 0 && dist < obsRange * 1.5;
      const online = !!u.csv;
      if (!first) {
        if (obs && !prev[u.k + 'obs']) {
          notify('critical', 'Object in front: ' + u.name, 'Obstacle detected ' + (dist > 0 ? dist + ' cm' : '') + ' ahead. The robot has stopped.',
            { modal: true, actions: [{ label: 'Emergency STOP', cls: 'danger', fn: () => sendManualCmd('MAN_STOP') }] });
        } else if (!obs && prev[u.k + 'obs']) {
          notify('ok', 'Path clear: ' + u.name, 'Obstacle removed.');
        } else if (near && !prev[u.k + 'near']) {
          notify('warn', 'Object approaching: ' + u.name, 'Something is ' + dist + ' cm ahead.');
        }
        if (u.trash === 'Full' && prev[u.k + 'trash'] !== 'Full') {
          notify('critical', 'Trash bin FULL: ' + u.name, 'Bin is at ' + u.trashCm + ' cm. Empty it soon.',
            { modal: true, actions: [{ label: 'Dispose now', cls: 'warn', fn: triggerDisposal }] });
        } else if (u.trash === 'Halfway' && prev[u.k + 'trash'] === 'Low') {
          notify('info', 'Bin half full: ' + u.name, 'Fill level has reached halfway.');
        }
        if (u.k === 's' && !online && prev.sOnline) notify('critical', u.name + ' offline', 'No data received from the slave unit.', { modal: true });
        if (u.k === 's' && online && prev.sOnline === false) notify('ok', u.name + ' online', 'Slave unit is reporting again.');
      }
      prev[u.k + 'obs'] = obs; prev[u.k + 'near'] = near; prev[u.k + 'trash'] = u.trash;
      if (u.k === 's') prev.sOnline = online;
    });

    if (!first) {
      if (data.lidOpen && !prev.lid) notify('info', 'Lid opened', 'Person detected - lid opened automatically.');
      if (data.manualMode !== prev.manual) {
        notify(data.manualMode ? 'warn' : 'info', data.manualMode ? 'Manual override ON' : 'Auto mode ON',
          data.manualMode ? 'Automatic routines are paused.' : 'Automatic routines resumed.', { modal: data.manualMode });
      }
      if (data.disposing && !prev.disposing) notify('warn', 'Disposal started', 'Robots are heading to the disposal area.', { push: true });
      if (!data.disposing && prev.disposing) notify('ok', 'Disposal complete', 'Robots are back at base.', { push: true });
      if (data.disposing && data.dispState !== prev.dispState) notify('info', 'Disposal: ' + (DISP_NAMES[data.dispState] || '-'), 'Stage ' + data.dispState + ' of the disposal cycle.');
    }
    prev.lid = data.lidOpen; prev.manual = data.manualMode; prev.disposing = data.disposing; prev.dispState = data.dispState;
    prev.init = true;
  }

  // --- scheduled-disposal reminders (uses the server clock) ---
  const schedFired = {};
  function checkSchedule() {
    if (!srvSynced || !lastData || !lastData.schedTime) return;
    const d = new Date((srvLocalEpoch + Math.floor((performance.now() - srvFetchedAt) / 1000)) * 1000);
    const hm = lastData.schedTime.split(':').map(Number);
    let diff = (hm[0] * 60 + hm[1]) - (d.getUTCHours() * 60 + d.getUTCMinutes());
    if (diff < 0) diff += 1440;
    const day = d.getUTCFullYear() + '-' + d.getUTCMonth() + '-' + d.getUTCDate() + '|' + lastData.schedTime;
    const lead = Math.max(1, parseInt(alertPrefs.leadMin) || 5);
    const fire = (tag, fn) => { const key = day + '|' + tag; if (!schedFired[key]) { schedFired[key] = true; fn(); } };
    if (diff === 0 && d.getUTCSeconds() < 30) {
      fire('now', () => notify('warn', 'Disposal time!', 'Scheduled disposal (' + lastData.schedTime + ') is starting now.', { modal: true, push: true }));
    } else if (diff === 1) {
      fire('1', () => notify('warn', 'Disposal in 1 minute', 'Keep the path clear. Starts at ' + lastData.schedTime + '.', { modal: true, push: true }));
    } else if (diff === lead && lead > 1) {
      fire('lead', () => notify('info', 'Disposal in ' + lead + ' minutes', 'Scheduled for ' + lastData.schedTime + '.', { push: true }));
    }
  }

  // ================= Calibration tab =================
  let calLoaded = false;
  function buildBars(id) {
    let h = '';
    for (let i = 0; i < 5; i++) {
      h += '<div class="sbar-row"><span>S' + (i + 1) + '</span><div class="sbar-track"><div class="sbar-fill" id="' + id + '_f' + i + '"></div>' +
           '<i class="mk-b" id="' + id + '_b' + i + '"></i><i class="mk-l" id="' + id + '_l' + i + '"></i></div>' +
           '<span class="data-value" id="' + id + '_v' + i + '">-</span></div>';
    }
    document.getElementById(id + '_bars').innerHTML = h;
  }
  function renderBars(id, csv) {
    const black = csv ? parseInt(csv[10]) : NaN, line = csv ? parseInt(csv[11]) : NaN;
    for (let i = 0; i < 5; i++) {
      const v = csv ? parseInt(csv[1 + i]) : NaN;
      const f = document.getElementById(id + '_f' + i);
      document.getElementById(id + '_v' + i).innerText = isNaN(v) ? '-' : v;
      f.style.width = isNaN(v) ? '0%' : (v / 1023 * 100) + '%';
      f.className = 'sbar-fill' + (isNaN(v) ? '' : (v <= black ? ' black' : (v < line ? ' line' : '')));
      document.getElementById(id + '_b' + i).style.left = isNaN(black) ? '-10px' : (black / 1023 * 100) + '%';
      document.getElementById(id + '_l' + i).style.left = isNaN(line) ? '-10px' : (line / 1023 * 100) + '%';
    }
    document.getElementById('cal_' + id + '_active').innerText = csv && csv.length > 13
      ? 'Active on Uno: black \u2264 ' + csv[10] + ', line < ' + csv[11] + ' (black < line, white > line), spin ' + csv[12] + ' ms, person ' + csv[13] + ' cm' + (csv.length > 16 ? ', PID ' + csv[14] + '/' + csv[15] + '/' + csv[16] : '')
      : 'Active on Uno: -';
  }
  function binText(cm, depth) {
    if (!(cm > 0)) return '-';
    const pct = Math.max(0, Math.min(100, Math.round((1 - cm / depth) * 100)));
    return cm + ' cm (' + pct + '% full)';
  }
  function renderCalib(data, m, s) {
    document.getElementById('cal_m_hdr').innerText = data.masterName + ' \u2013 Line Sensors';
    document.getElementById('cal_s_hdr').innerText = data.slaveName + ' \u2013 Line Sensors';
    renderBars('cal_m', m); renderBars('cal_s', s);
    const depth = (data.cal && data.cal.binDepth) || 40;
    document.getElementById('cal_m_dist').innerText = m ? (m[9] > 0 ? m[9] + ' cm' : 'no echo') : '-';
    document.getElementById('cal_s_dist').innerText = s ? (s[9] > 0 ? s[9] + ' cm' : 'no echo') : '-';
    document.getElementById('cal_m_bin').innerText = binText(data.masterTrashCm, depth);
    document.getElementById('cal_s_bin').innerText = binText(data.slaveTrashCm, depth);
    if (!calLoaded && data.cal) { loadCalFields(false); calLoaded = true; }
    if (syncFromUno && m && m.length > 13) {
      document.getElementById('calBlack').value = m[10];
      document.getElementById('calLine').value = m[11];
      syncFromUno = false;
    }
  }
  let syncFromUno = false;
  function loadCalFields(force) {
    if (!lastData || !lastData.cal) return;
    const c = lastData.cal;
    const set = (id, v) => { if (document.getElementById(id)) document.getElementById(id).value = v; };
    set('calBlack', c.black); set('calLine', c.line); set('calSpin', c.spin); set('calPerson', c.person);
    set('calDeg', c.dumpDeg); set('calHold', c.hold); set('calLid', c.lid); set('calBin', c.binDepth);
    set('calKp', c.kp); set('calKi', c.ki); set('calKd', c.kd);
    set('calCycles', c.cycles !== undefined ? c.cycles : 3);
    set('speedInp', c.speed); set('obsInp', c.obs); set('trashInp', c.trash);
    if (force) notify('info', 'Reloaded', 'Fields now show the values stored on the device.');
  }
  function saveCalibration() {
    const g = id => encodeURIComponent(document.getElementById(id).value);
    fetch('/set_calibration?black=' + g('calBlack') + '&line=' + g('calLine') + '&spin=' + g('calSpin') + '&person=' + g('calPerson') +
          '&deg=' + g('calDeg') + '&hold=' + g('calHold') + '&lid=' + g('calLid') + '&bin=' + g('calBin') +
          '&kp=' + g('calKp') + '&ki=' + g('calKi') + '&kd=' + g('calKd') + '&cyc=' + g('calCycles'))
      .then(r => r.text()).then(t => { notify('ok', 'Calibration saved', t); updateData(); });
  }
  function autoCal(mode) {
    const black = mode === 'black';
    showModal({
      level: 'info',
      title: black ? 'Calibrate on BLACK pad' : 'Calibrate on guide LINE',
      msg: black ? 'Make sure the robot is sitting fully on the all-black start/dump pad, then continue.'
                 : 'Make sure the center sensor (S3) is directly over the guide line, then continue.',
      actions: [{ label: 'Calibrate now', cls: 'warn', fn: () => {
        fetch('/calibrate?mode=' + mode).then(r => r.text()).then(t => {
          notify('ok', 'Calibration sent', t);
          setTimeout(() => { syncFromUno = true; updateData(); }, 1500);
        });
      } }]
    });
  }
</script>
</head>
<body>
  <div id="toastWrap" class="toast-wrap"></div>
  <div id="modalOverlay" class="modal-overlay">
    <div class="modal" id="modalBox">
      <div class="modal-icon" id="modalIcon">i</div>
      <h3 id="modalTitle"></h3>
      <p id="modalMsg"></p>
      <div class="modal-actions" id="modalActions"></div>
    </div>
  </div>

  <div id="drawerBackdrop" class="drawer-backdrop" onclick="closeSidebarDrawer()"></div>

  <!-- Top Sticky Navigation / Selection Bar -->
  <div class="top-app-bar">
    <div class="top-app-left">
      <button id="sidebarToggleBtn" class="menu-toggle-btn" onclick="toggleSidebar()">
        <span>☰</span> Menu
      </button>
      <div class="top-logo">
        <span>+</span> Med-Waste Swarm
      </div>
    </div>
    
    <div class="top-quick-select">
      <button id="topNavToggleBtn" class="primary" style="font-size:0.8rem; padding:0.4rem 0.75rem;" onclick="toggleTopNav()">
        <span>↕</span> Top Bar
      </button>
      <select id="navSelectDropdown" class="nav-select-dropdown" onchange="showSection(this.value)">
        <option value="overview">🏥 System Overview</option>
        <option value="manual">🎮 Manual Override</option>
        <option value="calib">🎚️ Calibration</option>
        <option value="terminals">💻 Serial Terminals</option>
        <option value="analytics">📊 Analytics &amp; Logs</option>
        <option value="config">⚙️ Configuration</option>
      </select>
    </div>
  </div>

  <!-- Sliding Horizontal Selection Strip (Hidable / Scrollable) -->
  <div id="topNavStripWrapper" class="top-nav-strip-wrapper">
    <div class="strip-header">
      <div class="strip-title">
        <span>⇄</span> Sliding Quick Selection
      </div>
      <div class="strip-scroll-controls">
        <button class="strip-arrow-btn" onclick="scrollNavStrip(-1)" title="Slide left">&larr;</button>
        <button class="strip-arrow-btn" onclick="scrollNavStrip(1)" title="Slide right">&rarr;</button>
      </div>
    </div>
    <div class="top-nav-strip" id="topNavStrip">
      <button class="top-pill-btn active" data-target="overview" onclick="showSection('overview')">🏥 System Overview</button>
      <button class="top-pill-btn" data-target="manual" onclick="showSection('manual')">🎮 Manual Override</button>
      <button class="top-pill-btn" data-target="calib" onclick="showSection('calib')">🎚️ Sensor Calibration</button>
      <button class="top-pill-btn" data-target="terminals" onclick="showSection('terminals')">💻 Serial Terminals</button>
      <button class="top-pill-btn" data-target="analytics" onclick="showSection('analytics')">📊 Analytics &amp; Logs</button>
      <button class="top-pill-btn" data-target="config" onclick="showSection('config')">⚙️ Configuration</button>
    </div>
  </div>

  <div class="layout">
    <header id="sidebarPanel" class="sidebar">
      <div class="sidebar-header-row">
        <h1 style="margin:0; font-size:1.35rem;"><span>+</span> Menu</h1>
        <button class="sidebar-close-btn" onclick="toggleSidebar()" title="Close / Hide">&times;</button>
      </div>
      <div class="clock-card">
        <div class="clock-time" id="clockTime">--:--:--</div>
        <div class="clock-date" id="clockDate">Time not set</div>
        <div class="clock-sync"><span id="clockSyncTxt">Not synced</span><button onclick="syncTime()">Sync</button></div>
      </div>
      <nav>
        <button class="nav-btn active" data-target="overview" onclick="showSection('overview')">🏥 System Overview</button>
        <button class="nav-btn" data-target="manual" onclick="showSection('manual')">🎮 Manual Override</button>
        <button class="nav-btn" data-target="calib" onclick="showSection('calib')">🎚️ Calibration</button>
        <button class="nav-btn" data-target="terminals" onclick="showSection('terminals')">💻 Serial Terminals</button>
        <button class="nav-btn" data-target="analytics" onclick="showSection('analytics')">📊 Analytics &amp; Logs</button>
        <button class="nav-btn" data-target="config" onclick="showSection('config')">⚙️ Configuration</button>
      </nav>
    </header>

    <main>
      <div id="globalAlert" style="display:none; background:#ffe0e8; color:#8a2346; padding:1rem; border:3px solid #000; border-radius: 10px; margin-bottom:1.5rem; font-weight:800; box-shadow: var(--shadow-sm);"></div>
      
      <div class="status-ribbon">
        <div id="slaveStatusPill" class="status-pill alert">
          <span class="status-dot-red"></span>
          <span id="slaveStatusTxt"><b>SLAVE ROBOT: DISCONNECTED</b> &ndash; Waiting for slave...</span>
        </div>
        <div class="status-pill" style="background:#e8f4fd;">
          <span>🌐</span>
          <span><b>Local URL:</b> <a href="http://medwaste.local" target="_blank" style="color:#000; text-decoration:underline;">http://medwaste.local</a></span>
          <button onclick="navigator.clipboard.writeText('http://medwaste.local'); notify('ok','Copied','URL copied to clipboard!')" style="border:2px solid #000; border-radius:6px; background:#fff; font-size:0.72rem; font-weight:800; cursor:pointer; padding:0.15rem 0.45rem;">Copy</button>
        </div>
        <div class="status-pill" style="background:var(--card-pink);">
          <span>📶</span>
          <span><b>Fallback AP:</b> <code id="pillApSsid">MedWaste-Master-AP</code> (<span id="pillApPass">pass: medwaste123</span>)</span>
        </div>
      </div>
      
      <div id="overview" class="section active">
        <h2>System Overview</h2>
        
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.8rem; margin:1rem 0 0.8rem 0;">
          <h3 style="margin:0;">Swarm Fleet Telemetry (<span id="swarmCountBadge">1</span> Active Units)</h3>
          <span class="tag accent">Autonomous Medical Waste Swarm</span>
        </div>
        <div class="panel-grid" id="swarmTelemetryGrid">
          <div class="panel">
            <h4 id="m_hdr" style="margin:0 0 1rem 0; font-size:1.2rem; font-weight:800;">Unit Alpha</h4>
            <ul class="data-list">
              <li><span class="data-label">Operational State</span> <span id="m_state" class="data-value">-</span></li>
              <li><span class="data-label">Motor Speed</span> <span id="m_speed" class="data-value">-</span></li>
              <li><span class="data-label">Sensor Array</span> <span id="m_sensors" class="data-value">-</span></li>
              <li><span class="data-label">Front Ultrasonic</span> <span id="m_obs_dist" class="data-value">-</span></li>
              <li><span class="data-label">Capacity</span> <span id="m_trash" class="data-value">-</span></li>
            </ul>
          </div>
        </div>

        <h3 style="margin-top:2.5rem;">Quick Actions</h3>
        <div class="panel-grid">
          <div class="panel" style="background:var(--panel-bg);">
            <p style="margin: 0 0 1rem 0; font-size: 0.9rem; font-weight:600;">Force immediate automated disposal cycle (Leave all-black start &rarr; follow line &rarr; detect all-black dump &rarr; spin opposite &rarr; multi-tilt shake &rarr; return).</p>
            <button class="accent" onclick="triggerDisposal()">Execute Auto Disposal</button>
          </div>
        </div>
      </div>

      <div id="terminals" class="section">
        <h2>Live Serial Monitors</h2>
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:1rem; margin-bottom:1.25rem;">
          <div class="hint">Bi-directional diagnostic monitor streaming Master ESP32-C3 console and Arduino Uno dedicated UART pins (D2/D12).</div>
          <div style="display:flex; gap:0.6rem; align-items:center;">
            <label class="switch-row" style="margin:0;"><input type="checkbox" id="termAutoScroll" checked> <span>Auto-scroll</span></label>
            <button class="primary" onclick="clearTerminals()">Clear Display</button>
          </div>
        </div>

        <div class="panel-grid" style="grid-template-columns: repeat(auto-fit, minmax(360px, 1fr));">
          <div class="panel" style="background:#14131a; border:3px solid #000; box-shadow:var(--shadow);">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.6rem; border-bottom:2px dashed #444; padding-bottom:0.4rem;">
              <span style="font-family:var(--font-head); font-weight:700; color:#ff8fc2; font-size:1.05rem;">ESP32 Controller Terminal</span>
              <span class="tag active" style="font-size:0.7rem; padding:0.15rem 0.5rem;">ONLINE</span>
            </div>
            <div id="espConsole" class="terminal-box">Waiting for ESP32 logs...</div>
          </div>

          <div class="panel" style="background:#14131a; border:3px solid #000; box-shadow:var(--shadow);">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.6rem; border-bottom:2px dashed #444; padding-bottom:0.4rem;">
              <span style="font-family:var(--font-head); font-weight:700; color:#a8f0ff; font-size:1.05rem;">Arduino Uno UART Stream</span>
              <span class="tag accent" style="font-size:0.7rem; padding:0.15rem 0.5rem;">9600 BAUD</span>
            </div>
            <div id="unoConsole" class="terminal-box">Waiting for Uno packets...</div>
          </div>
        </div>

        <h3 style="margin-top:1.8rem;">Direct Uno Command Sender</h3>
        <div class="panel" style="background:var(--panel-bg);">
          <div style="display:flex; gap:0.6rem; flex-wrap:wrap; align-items:center; margin-bottom:0.8rem;">
            <input type="text" id="unoCmdInput" placeholder="Enter Uno command (e.g. MAN_FWD, MAN_STOP, CAL_BLACK, PID,45,0,60)" style="max-width:480px; flex:1;">
            <button class="accent" onclick="sendDirectUnoCmd()">Send to Uno</button>
          </div>
          <div style="display:flex; gap:0.5rem; flex-wrap:wrap;">
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('MAN_STOP')">STOP</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('MAN_FWD')">FORWARD</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('MAN_BWD')">BACKWARD</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('MAN_LEFT')">LEFT</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('MAN_RIGHT')">RIGHT</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('MODE_AUTO')">AUTO MODE</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('CAL_BLACK')">CAL BLACK</button>
            <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.7rem;" onclick="sendQuickUno('CAL_LINE')">CAL LINE</button>
          </div>
        </div>
      </div>

      <div id="manual" class="section">
        <h2>Manual Override &amp; Teleoperation</h2>
        <div class="panel" style="margin-bottom: 1.5rem;">
          <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:1rem;">
            <div>
              <strong>Current Control Mode: </strong> <span id="modeBadge"><span class="tag active">AUTO MODE</span></span>
            </div>
            <div style="display:flex; gap:0.75rem;">
              <button class="accent" onclick="sendManualCmd('MODE_AUTO')">Switch to AUTO</button>
              <button class="warn" onclick="sendManualCmd('MODE_MANUAL')">Switch to MANUAL OVERRIDE</button>
            </div>
          </div>
        </div>

        <div class="panel-grid">
          <div class="panel">
            <h4 style="margin:0 0 0.5rem 0; font-size:1.1rem; font-weight:800;">Drive Override</h4>
            <p style="margin:0 0 1rem 0; font-size:0.85rem;">Pressing any drive button automatically engages Manual Override.</p>
            <div class="dpad">
              <div></div>
              <button onclick="sendManualCmd('MAN_FWD')">&uarr;<br>FWD</button>
              <div></div>
              <button onclick="sendManualCmd('MAN_LEFT')">&larr;<br>LEFT</button>
              <button class="stop-btn" onclick="sendManualCmd('MAN_STOP')">STOP</button>
              <button onclick="sendManualCmd('MAN_RIGHT')">&rarr;<br>RIGHT</button>
              <div></div>
              <button onclick="sendManualCmd('MAN_BWD')">&darr;<br>BWD</button>
              <div></div>
            </div>
          </div>

          <div class="panel">
            <h4 style="margin:0 0 1rem 0; font-size:1.1rem; font-weight:800;">Bin Lid &amp; Stepper Tilt (-20&deg; / 90&deg;)</h4>
            <div style="display:flex; flex-direction:column; gap:0.75rem;">
              <div style="display:flex; gap:0.75rem; flex-wrap:wrap;">
                <button class="primary" onclick="sendManualCmd('LID_OPEN')">Open Lid</button>
                <button class="primary" onclick="sendManualCmd('LID_CLOSE')">Close Lid</button>
              </div>
              <hr style="border:2px dashed #000; width:100%;">
              <div style="display:flex; gap:0.75rem; flex-wrap:wrap;">
                <button class="warn" onclick="sendManualCmd('TILT_DUMP')">Tilt to -20&deg; (Dump)</button>
                <button class="accent" onclick="sendManualCmd('TILT_UPRIGHT')">Return to 90&deg; (Upright)</button>
              </div>
            </div>
          </div>
        </div>
      </div>


      <div id="calib" class="section">
        <h2>Sensor Calibration</h2>

        <h3>Live Sensor Readings</h3>
        <div class="panel-grid">
          <div class="panel">
            <h4 id="cal_m_hdr" style="margin:0 0 0.6rem 0;">Unit Alpha &ndash; Line Sensors</h4>
            <div id="cal_m_bars"></div>
            <div class="legend"><span class="dot" style="background:#1a1a1a"></span>Solid Black (&le; black thresh)<span class="dot" style="background:#2f8fd8"></span>Black Line (&lt; line thresh)</div>
            <div class="hint" id="cal_m_active">Active on Uno: -</div>
          </div>
          <div class="panel">
            <h4 id="cal_s_hdr" style="margin:0 0 0.6rem 0;">Unit Beta &ndash; Line Sensors</h4>
            <div id="cal_s_bars"></div>
            <div class="legend"><span class="dot" style="background:#1a1a1a"></span>Solid Black (&le; black thresh)<span class="dot" style="background:#2f8fd8"></span>Black Line (&lt; line thresh)</div>
            <div class="hint" id="cal_s_active">Active on Uno: -</div>
          </div>
        </div>

        <div class="panel-grid" style="margin-top:1.25rem;">
          <div class="panel">
            <h4 style="margin:0 0 0.6rem 0;">Ultrasonic Sensors</h4>
            <ul class="data-list">
              <li><span class="data-label">Front (Alpha)</span> <span id="cal_m_dist" class="data-value">-</span></li>
              <li><span class="data-label">Front (Beta)</span> <span id="cal_s_dist" class="data-value">-</span></li>
              <li><span class="data-label">Bin level (Alpha)</span> <span id="cal_m_bin" class="data-value">-</span></li>
              <li><span class="data-label">Bin level (Beta)</span> <span id="cal_s_bin" class="data-value">-</span></li>
            </ul>
          </div>
          <div class="panel">
            <h4 style="margin:0 0 0.6rem 0;">Quick Auto-Calibration</h4>
            <p class="hint" style="margin-top:0;">Place the robot on the surface first, then press. Thresholds are measured from the live readings and sent to both units.</p>
            <div style="display:flex; gap:0.6rem; flex-wrap:wrap; margin-top:0.8rem;">
              <button class="primary" onclick="autoCal('black')">Calibrate on BLACK pad</button>
              <button class="primary" onclick="autoCal('line')">Calibrate on guide LINE</button>
            </div>
          </div>
        </div>

        <h3>Calibration Parameters</h3>
        <div class="panel">
          <div class="cal-grid">
            <div class="control-group"><label>Black-pad threshold (0&ndash;1023)</label><input type="number" id="calBlack" min="0" max="1023"><div class="hint">All 5 sensors at or below this = start/dump pad (black &le; threshold)</div></div>
            <div class="control-group"><label>Line-detect threshold (0&ndash;1023)</label><input type="number" id="calLine" min="0" max="1023"><div class="hint">Below this = Black Line; Above this = White Floor</div></div>
            <div class="control-group"><label>Spin-around time (ms)</label><input type="number" id="calSpin" min="200" max="10000"><div class="hint">Time to turn ~180&deg; at the dump area</div></div>
            <div class="control-group"><label>Person detect range (cm)</label><input type="number" id="calPerson" min="5" max="400"><div class="hint">Opens the lid when someone is closer</div></div>
            <div class="control-group"><label>Dump tilt angle (&deg;)</label><input type="number" id="calDeg" min="-60" max="90"><div class="hint">90&deg; = upright, default &minus;20&deg;</div></div>
            <div class="control-group"><label>Tilt hold time (ms)</label><input type="number" id="calHold" min="200" max="30000"><div class="hint">How long it stays tilted</div></div>
            <div class="control-group"><label>Lid open time (ms)</label><input type="number" id="calLid" min="1000" max="60000"><div class="hint">Auto-close delay</div></div>
            <div class="control-group"><label>Bin depth (cm)</label><input type="number" id="calBin" min="5" max="400"><div class="hint">Sensor to empty-bin floor, for fill %</div></div>
            <div class="control-group"><label>PID &ndash; Kp (steering strength)</label><input type="number" id="calKp" min="0" max="500" step="0.5"><div class="hint">Higher = sharper turns, too high = wobble</div></div>
            <div class="control-group"><label>PID &ndash; Ki (drift correction)</label><input type="number" id="calKi" min="0" max="50" step="0.01"><div class="hint">Keep at 0 unless it drifts to one side</div></div>
            <div class="control-group"><label>PID &ndash; Kd (smoothing)</label><input type="number" id="calKd" min="0" max="500" step="0.5"><div class="hint">Higher = smoother, damps jitter</div></div>
            <div class="control-group"><label>Waste ejection shakes (cycles)</label><input type="number" id="calCycles" min="1" max="10" value="3"><div class="hint">Number of times stepper shakes at -20&deg; to eject 100% of trash</div></div>
          </div>
          <div style="display:flex; gap:0.6rem; flex-wrap:wrap; margin-top:0.5rem;">
            <button class="accent" onclick="saveCalibration()">Save &amp; Apply to Both Units</button>
            <button class="primary" onclick="loadCalFields(true)">Reload from Device</button>
          </div>
          <div class="hint" style="margin-top:0.8rem;">Obstacle range, base speed and capacity alert are in the Configuration tab. All values are saved and restored after a reboot.</div>
        </div>
      </div>
      <div id="analytics" class="section">
        <h2>Analytics & Logs</h2>
        
        <h3>Disposal Frequency</h3>
        <div class="panel">
          <canvas id="disposalChart"></canvas>
        </div>

        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; margin-top:1.8rem; margin-bottom:0.6rem;">
          <h3 style="margin:0;">Persistent Disposal History (Saved in Internal Flash ROM)</h3>
          <button class="primary" style="font-size:0.8rem; padding:0.3rem 0.8rem;" onclick="clearHistory()">Clear History</button>
        </div>
        <div class="log-viewer" id="history">
          Waiting for data stream...
        </div>
        <h3>Alert History</h3>
        <div class="panel" id="alertHistory" style="max-height:260px; overflow-y:auto;">
          <div class="hint">No alerts yet.</div>
        </div>

      </div>

      <div id="config" class="section">
        <h2>Configuration</h2>
        
        <h3>Unit Identification</h3>
        <div style="max-width: 400px; padding: 1.5rem; border: 3px solid #000; border-radius: 12px; box-shadow: var(--shadow); background: var(--panel-bg);">
          <div class="control-group">
            <label>Master Unit Name</label>
            <input type="text" id="nameInpM" value="Unit Alpha">
          </div>
          <div class="control-group">
            <label>Slave Unit Name</label>
            <input type="text" id="nameInpS" value="Unit Beta">
          </div>
          <button class="primary" onclick="updateNames()">Update Names</button>
        </div>
        
        <h3 style="margin-top:2.5rem;">Automated Scheduling</h3>
        <div style="max-width: 400px; padding: 1.5rem; border: 3px solid #000; border-radius: 12px; box-shadow: var(--shadow); background: var(--card-pink);">
          <div class="control-group">
            <label>Current Scheduled Time: <span id="schedInfo" style="font-weight:800; font-family:monospace; background:#fff; padding:0.2rem 0.5rem; border:3px solid #000; border-radius:8px;">--:--</span></label>
            <div style="display:flex; gap:0.5rem; margin-top:1rem;">
              <input type="time" id="schedTime" style="margin:0;">
              <button class="primary" onclick="setSchedule()">Update</button>
            </div>
          </div>
        </div>

        <h3 style="margin-top:2.5rem;">Hardware Thresholds</h3>
        <div style="max-width: 400px; padding: 1.5rem; border: 3px solid #000; border-radius: 12px; box-shadow: var(--shadow); background: var(--panel-bg);">
          <div class="control-group">
            <label>Base Motor Speed (PWM 0-255)</label>
            <input type="number" id="speedInp" value="150" min="0" max="255">
          </div>
          <div class="control-group">
            <label>Obstacle Detection Range (cm)</label>
            <input type="number" id="obsInp" value="30">
          </div>
          <div class="control-group">
            <label>Capacity Alert Threshold (cm)</label>
            <input type="number" id="trashInp" value="10">
          </div>
          <button class="primary" onclick="updateSettings()">Commit Parameters</button>
        </div>
        <h3 style="margin-top:2.5rem;">Alert Preferences</h3>
        <div style="max-width: 400px; padding: 1.5rem; border: 3px solid #000; border-radius: 12px; box-shadow: var(--shadow); background: var(--panel-bg);">
          <div class="switch-row"><input type="checkbox" id="prefModal" onchange="applyPrefs()"><label for="prefModal">Pop-up (modal) alerts for critical events</label></div>
          <div class="switch-row"><input type="checkbox" id="prefSound" onchange="applyPrefs()"><label for="prefSound">Alert sound</label></div>
          <div class="switch-row"><input type="checkbox" id="prefBrowser" onchange="toggleBrowserNotif(this)"><label for="prefBrowser">Browser notifications</label></div>
          <div class="hint" id="notifStatus"></div>
          <div class="control-group" style="margin-top:1rem;">
            <label>Remind me before scheduled disposal (minutes)</label>
            <input type="number" id="prefLead" min="1" max="60" onchange="applyPrefs()">
          </div>
          <button class="primary" onclick="notify('critical','Test alert','This is how critical alerts look.',{modal:true})">Send Test Alert</button>
        </div>

        <h3 style="margin-top:2.5rem;">Wi-Fi &amp; Network Setup</h3>
        <div style="max-width: 440px; padding: 1.5rem; border: 3px solid #000; border-radius: 12px; box-shadow: var(--shadow); background: var(--panel-bg);">
          <div style="margin-bottom: 1rem;">
            <strong>Network Status: </strong>
            <span id="wifiStatusBadge"><span class="tag active">Checking...</span></span>
          </div>
          <div style="font-size: 0.9rem; margin-bottom: 1rem; color: var(--text-muted);">
            <div><b>Active SSID:</b> <span id="curWifiSsid" style="font-family:var(--font-mono); color:#000;">-</span></div>
            <div><b>IP Address:</b> <span id="curWifiIp" style="font-family:var(--font-mono); color:#000;">-</span></div>
          </div>
          <div style="display:flex; gap:0.5rem; margin-bottom:1.2rem; flex-wrap:wrap;">
            <button class="primary" onclick="scanWifi()">Scan Nearby Wi-Fi</button>
            <button class="danger" onclick="resetWifi()">Reset to Hotspot</button>
          </div>
          <div id="wifiScanResults" style="display:none; margin-bottom:1.2rem;">
            <label style="display:block; font-size:0.9rem; font-weight:800; margin-bottom:0.3rem;">Detected Networks (click to select):</label>
            <div id="wifiList" style="max-height:140px; overflow-y:auto; border:2px solid #000; border-radius:8px; padding:0.5rem; background:var(--card-pink);"></div>
          </div>
          <div class="control-group">
            <label>Wi-Fi Name (SSID)</label>
            <input type="text" id="wifiSsidInput" placeholder="Enter or select Wi-Fi name">
          </div>
          <div class="control-group">
            <label>Wi-Fi Password</label>
            <input type="text" id="wifiPassInput" placeholder="Enter Wi-Fi password">
          </div>
          <div class="control-group">
            <label>ESP32 Hotspot Password (WPA2)</label>
            <input type="text" id="apPassInput" placeholder="min 8 chars (e.g. medwaste123)" value="medwaste123">
            <div class="hint">Password for direct connection to <b>MedWaste-Master-AP</b> when out of router range.</div>
          </div>
          <div style="display:flex; gap:0.6rem; flex-wrap:wrap; margin-top:0.8rem;">
            <button class="accent" onclick="saveWifi()">Save Wi-Fi &amp; Connect</button>
            <button class="primary" onclick="saveApPassOnly()">Update Hotspot Password Only</button>
          </div>
          <div class="hint" style="margin-top:0.8rem; line-height:1.45; border-top:2px dashed #000; padding-top:0.6rem;">
            <b>Connecting when you leave this Wi-Fi network:</b><br>
            If you take the robot to another room, building, or outdoors where this Wi-Fi is unavailable, the ESP32 automatically keeps broadcasting its fallback hotspot: <b>MedWaste-Master-AP</b>.<br>
            Connect your phone or laptop to it using your known password (default: <code>medwaste123</code>), then simply open <b><a href="http://medwaste.local" target="_blank">http://medwaste.local</a></b> or <b>http://192.168.4.1</b> in any browser to control the robot or connect to a new Wi-Fi!
          </div>
        </div>

      </div>

    </main>
  </div>

</body>
</html>
)rawliteral";
