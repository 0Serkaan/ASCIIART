import React, { useState } from 'react';
import { connectToStream } from '../api/streamApi';

export default function ChannelSelector({ onConnect }) {
  const [host, setHost] = useState('localhost');
  const [port, setPort] = useState('12345');
  const [channel, setChannel] = useState('1');
  const [clientId, setClientId] = useState('client1');

  const handleConnect = async () => {
    const res = await connectToStream(host, port, channel, clientId);
    if (res.ok) onConnect(clientId);
    else alert('Bağlantı hatası');
  };

  return (
    <div>
      <p>Bağlantı bilgileri:</p>
      <input value={host} onChange={(e) => setHost(e.target.value)} placeholder="host" />
      <input value={port} onChange={(e) => setPort(e.target.value)} placeholder="port" />
      <input value={channel} onChange={(e) => setChannel(e.target.value)} placeholder="kanal" />
      <input value={clientId} onChange={(e) => setClientId(e.target.value)} placeholder="clientId" />
      <button onClick={handleConnect}>Bağlan</button>
    </div>
  );
}