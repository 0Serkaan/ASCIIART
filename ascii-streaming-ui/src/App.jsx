import React, { useState, useEffect } from 'react';
import ChannelSelector from './components/ChannelSelector';
import Controls from './components/Controls';
import StreamViewer from './components/StreamViewer';

export default function App() {
  const [clientId, setClientId] = useState('');
  const [isConnected, setIsConnected] = useState(false);

  return (
    <div style={{ padding: '2rem', fontFamily: 'monospace' }}>
      <h1>ASCII Streaming Viewer</h1>
      {!isConnected ? (
        <ChannelSelector
          onConnect={(id) => {
            setClientId(id);
            setIsConnected(true);
          }}
        />
      ) : (
        <>
          <Controls clientId={clientId} />
          <StreamViewer clientId={clientId} />
        </>
      )}
    </div>
  );
}