import React, { useEffect, useState } from 'react';
import { readFrame } from '../api/streamApi';

export default function StreamViewer({ clientId }) {
  const [frame, setFrame] = useState('');
  const [lastFrame, setLastFrame] = useState('');

  useEffect(() => {
    const interval = setInterval(async () => {
      const text = await readFrame(clientId);

      if (text && text !== lastFrame) {
        setFrame(text);
        setLastFrame(text);
      }

      if (text === null) {
        console.log("[INFO] Pause veya timeout sebebiyle frame gelmedi.");
        
      }
    }, 50); // 50ms (20fps)

    return () => clearInterval(interval);
  }, [clientId, lastFrame]);

  return (
    <div style={{ height: '480px', overflow: 'hidden', background: '#000' }}>
      <pre style={{
        margin: 0,
        fontFamily: 'monospace',
        fontSize: '12px',
        color: '#0f0',
        lineHeight: '1em',
        whiteSpace: 'pre',
      }}>
        {frame}
      </pre>
    </div>
  );
}
