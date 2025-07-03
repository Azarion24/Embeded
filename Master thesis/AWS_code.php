<?php
$firebase_base_url = "Your URL to Firebase database";
$firebase_secret   = "Your Firebase secret key";

// Odbierz dane JSON z POST
$json_data = file_get_contents('php://input');
if(!$json_data) {
    http_response_code(400);
    die("Brak danych");
}

$data = json_decode($json_data, true);
if(!$data) {
    http_response_code(400);
    die("Niepoprawny JSON");
}

// Rozpoznaj typ i przygotuj ścieżkę docelową
if (isset($data['type'])) {
    switch($data['type']) {
        case 'currentLocation':
            $url = $firebase_base_url . "/currentLocation.json?auth=" . $firebase_secret;
            unset($data['type']);
            break;
        case 'currentStatus':
            $url = $firebase_base_url . "/currentStatus.json?auth=" . $firebase_secret;
            unset($data['type']);
            $data['lastUpdated'] = date('c');
            break;
        case 'lastParking':
            $url = $firebase_base_url . "/lastParking.json?auth=" . $firebase_secret;
            unset($data['type']);
            break;
        case 'tripBegin':
            $url = $firebase_base_url . "/trip/begin.json?auth=" . $firebase_secret;
            unset($data['type']);
            break;
        case 'tripEnd':
            $url = $firebase_base_url . "/trip/end.json?auth=" . $firebase_secret;
            unset($data['type']);
            break;
        default:
            http_response_code(400);
            die("Nieznany typ danych");
    }
} 

// Wysyłka do Firebase metodą PUT
$options = [
    "http" => [
        "method"  => "PUT",
        "header"  => "Content-Type: application/json",
        "content" => json_encode($data)
    ]
];

$context = stream_context_create($options);
$result  = file_get_contents($url, false, $context);

if ($result === FALSE) {
    http_response_code(500);
    echo "Błąd wysyłania do Firebase";
} else {
    echo "OK";
}
?>