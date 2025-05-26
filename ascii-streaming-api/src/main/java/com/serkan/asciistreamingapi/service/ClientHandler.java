package com.serkan.asciistreamingapi.service;

import java.io.*;
import java.net.Socket;
import java.net.SocketTimeoutException;

public class ClientHandler {
    private final String host;
    private final int port;
    private final int channel;
    private Socket socket;
    private BufferedWriter writer;

    // Sunucu bilgilerini ve kanal numarasını alır
    public ClientHandler(String host, int port, int channel) {
        this.host = host;
        this.port = port;
        this.channel = channel;
    }

    // Sunucuya bağlanma işlemi
    public void connect() throws IOException {
        socket = new Socket(host, port);
        socket.setSoTimeout(500); // 500ms timeout verildi, pause durumlarında zaman aşımı için

        writer = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), "UTF-8"));

        // İlk olarak kanal numarası gönderiliyor
        sendRawLine(String.valueOf(channel));
    }

    // Komutları sunucuya iletmek için kullanılır
    public void sendCommand(String cmd) {
        if (writer != null) {
            try {
                System.out.println("[DEBUG] Komut gönderiliyor: " + cmd);
                sendRawLine(cmd);
            } catch (IOException e) {
                System.err.println("[HATA] Komut gönderim hatası: " + e.getMessage());
            }
        }
    }

    // Satır sonu ile birlikte komutu doğrudan yazıcıya gönderir
    private void sendRawLine(String line) throws IOException {
        writer.write(line);
        writer.write("\n");
        writer.flush();
    }

    // Sunucudan gelen frame'i okur
    public String readFrame() {
        try {
            InputStream inStream = socket.getInputStream();

            // İlk 4 baytlık header okunuyor (frame uzunluğu)
            byte[] header = inStream.readNBytes(4);
            if (header.length < 4) {
                System.err.println("[HATA] Header eksik geldi (length=" + header.length + ")");
                return null;
            }

            String headerStr = new String(header).trim();
            if (!headerStr.matches("\\d+")) {
                System.err.println("[HATA] Geçersiz header: '" + headerStr + "'");
                return null;
            }

            int frameLength = Integer.parseInt(headerStr);
            if (frameLength <= 0 || frameLength > 2048) {
                System.err.println("[HATA] Frame uzunluğu geçersiz: " + frameLength);
                return null;
            }

            // Belirtilen uzunluk kadar frame verisi okunuyor
            byte[] frameBytes = inStream.readNBytes(frameLength);
            if (frameBytes.length != frameLength) {
                System.err.println("[HATA] Frame eksik okundu.");
                return null;
            }

            String frame = new String(frameBytes);
            // Sadece boşluklardan oluşan frame'ler de atlanıyor
            if (frame.trim().isEmpty() || frame.chars().allMatch(Character::isWhitespace)) {
                System.out.println("[INFO] Frame içeriği boş/boşluk (muhtemelen pause).");
                return null;
            }

            System.out.println(">>> Frame satır sayısı: " + frame.split("\n", -1).length);
            return frame;

        } catch (SocketTimeoutException e) {
            System.out.println("[INFO] readFrame timeout — pause durumunda olabilir.");
            return null;
        } catch (IOException e) {
            System.err.println("[HATA] readFrame exception: " + e.getMessage());
            return null;
        }
    }

    // Bağlantı sonlandırılır
    public void close() {
        try {
            if (socket != null) socket.close();
        } catch (IOException ignored) { }
    }
}
