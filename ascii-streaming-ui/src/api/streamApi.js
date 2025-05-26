const API_BASE = 'http://localhost:8080/api/stream';

export const connectToStream = async (host, port, channel, clientId) => {
  return await fetch(`${API_BASE}/connect?host=${host}&port=${port}&channel=${channel}&clientId=${clientId}`, {
    method: 'POST',
  });
};

export const sendCommand = async (clientId, command) => {
  await fetch(`${API_BASE}/command?clientId=${clientId}&command=${command}`, {
    method: 'POST',
  });
};

export const readFrame = async (clientId) => {
  const res = await fetch(`${API_BASE}/read?clientId=${clientId}`);
  if (!res.ok) return null;
  const text = await res.text();
  return text.trim() === '' ? null : text;
};
