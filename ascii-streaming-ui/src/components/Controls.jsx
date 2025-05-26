import React from 'react';
import { sendCommand } from '../api/streamApi';

export default function Controls({ clientId }) {
  const buttons = [
    { label: '▶️ Play', value: '1' },
    { label: '⏸️ Pause', value: '2' },
    { label: '🔁 Resume', value: '3' },
    { label: '🔄 Restart', value: '4' },
    { label: '⏩ x2', value: '5' },
    { label: '⏩⏩ x4', value: '6' },
  ];

  return (
    <div style={{ margin: '1rem 0' }}>
      {buttons.map((btn) => (
        <button key={btn.value} onClick={() => sendCommand(clientId, btn.value)}>
          {btn.label}
        </button>
      ))}
    </div>
  );
}
