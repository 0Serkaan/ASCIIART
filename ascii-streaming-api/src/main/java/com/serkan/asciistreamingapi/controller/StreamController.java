package com.serkan.asciistreamingapi.controller;

import com.serkan.asciistreamingapi.service.ClientHandler;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@CrossOrigin(origins = "http://localhost:5173") // Frontend CORS izinleri
@RestController
@RequestMapping("/api/stream") // Tüm endpoint’ler bu path altında gruplanır
public class StreamController {

    // Her bir clientId’ye karşılık gelen aktif handler’lar burada tutulur
    private final Map<String, ClientHandler> clients = new ConcurrentHashMap<>();

    // Sunucuya bağlantı kurulmasını sağlayan endpoint
    @PostMapping("/connect")
    public ResponseEntity<String> connect(@RequestParam String host,
                                          @RequestParam int port,
                                          @RequestParam int channel,
                                          @RequestParam String clientId) {
        try {
            // Aynı client tekrar bağlanmaya çalışıyorsa engelle
            if (clients.containsKey(clientId)) {
                return ResponseEntity.badRequest().body("Client already connected.");
            }

            // Yeni bir handler oluşturup bağlan
            ClientHandler handler = new ClientHandler(host, port, channel);
            handler.connect();
            clients.put(clientId, handler);

            return ResponseEntity.ok("Connection successful.");
        } catch (Exception e) {
            // Hata durumunda detaylı mesaj döndür
            return ResponseEntity.internalServerError().body("Connection error: " + e.getMessage());
        }
    }

    // Komut göndermek için kullanılan endpoint
    @PostMapping("/command")
    public ResponseEntity<String> sendCommand(@RequestParam String clientId,
                                              @RequestParam String command) {
        ClientHandler handler = clients.get(clientId);
        if (handler == null) {
            return ResponseEntity.badRequest().body("Client not found.");
        }

        // İlgili handler üzerinden komut iletiliyor
        handler.sendCommand(command);
        return ResponseEntity.ok("Command sent: " + command);
    }

    // Frame verisini sunucudan okur ve frontend’e gönderir
    @GetMapping("/read")
    public ResponseEntity<String> readFrame(@RequestParam String clientId) {
        ClientHandler handler = clients.get(clientId);
        if (handler == null) {
            return ResponseEntity.badRequest().body("Client not found.");
        }

        String frame = handler.readFrame();

        // Eğer frame boşsa HTTP 204 No Content döner
        if (frame == null || frame.isBlank()) {
            return ResponseEntity.noContent().build();
        }

        return ResponseEntity.ok(frame);
    }

    // Bağlantıyı sonlandırır ve kaynakları temizler
    @DeleteMapping("/disconnect")
    public ResponseEntity<String> disconnect(@RequestParam String clientId) {
        ClientHandler handler = clients.remove(clientId);
        if (handler != null) {
            handler.close();
            return ResponseEntity.ok("Client disconnected.");
        }
        return ResponseEntity.badRequest().body("Client not found.");
    }
}
