<?php
header('Content-Type: application/json');
header('Cache-Control: no-store');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { exit; }

$stateFile = __DIR__ . '/state.json';

function default_state() {
  return [
    'ch1' => [
      'status' => 'ok', 'vset' => 100, 'iset' => 1000,
      'vout' => 100, 'iout' => 1000, 'powerout' => 100000,
      'outputenable' => 1
    ],
    'ch2' => [
      'status' => 'ok', 'vset' => 500, 'iset' => 1500,
      'vout' => 400, 'iout' => 1500, 'powerout' => 600000,
      'outputenable' => 1
    ],
    'all' => ['status' => 'ok', 'trak' => 'independent']
  ];
}

if (!file_exists($stateFile)) {
  file_put_contents($stateFile, json_encode(default_state(), JSON_PRETTY_PRINT));
}
$state = json_decode(@file_get_contents($stateFile), true);
if (!$state) $state = default_state();

/* -------- POST: merge updates and persist (with optional track link) -------- */
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
  $update = json_decode(file_get_contents('php://input'), true) ?? [];

  // Merge posted fields
  foreach ($update as $key => $payload) {
    if (isset($state[$key]) && is_array($payload)) {
      $state[$key] = array_merge($state[$key], $payload);
    }
  }

  // If tracking is ON, mirror changed vset/iset/outputenable across channels
  $link = isset($state['all']['trak']) && strtolower($state['all']['trak']) === 'link';
  if ($link) {
    foreach (['vset','iset','outputenable'] as $f) {
      if (isset($update['ch1'][$f]) && !isset($update['ch2'][$f])) $state['ch2'][$f] = $state['ch1'][$f];
      if (isset($update['ch2'][$f]) && !isset($update['ch1'][$f])) $state['ch1'][$f] = $state['ch2'][$f];
    }
  }

  file_put_contents($stateFile, json_encode($state, JSON_PRETTY_PRINT));
  echo json_encode($state);
  exit;
}

/* -------- GET: simulate live readings with random noise -------- */
function step_channel(&$ch) {
  // If output is OFF, decay toward 0 quickly with a bit of noise
  if (empty($ch['outputenable'])) {
    $ch['vout'] = max(0, $ch['vout'] - intdiv($ch['vout'], 4) + rand(-20, 5));   // mV
    $ch['iout'] = max(0, $ch['iout'] - intdiv($ch['iout'], 4) + rand(-50, 10));  // mA
  } else {
    // Move gently toward setpoints + small noise
    $targetV = (int)($ch['vset'] ?? 0);
    $targetI = (int)($ch['iset'] ?? 0);

    $ch['vout'] += (int)round(($targetV - $ch['vout']) * 0.15); // approach setpoint
    $ch['iout'] += (int)round(($targetI - $ch['iout']) * 0.15);

    $ch['vout'] += rand(-8, 8);   // ±8 mV noise
    $ch['iout'] += rand(-16, 16); // ±16 mA noise

    $ch['vout'] = max(0, $ch['vout']);
    $ch['iout'] = max(0, $ch['iout']);
  }

  // Power in mW (mV * mA / 1000)
  $ch['powerout'] = (int) round(($ch['vout'] * $ch['iout']) / 1000);
  $ch['status'] = 'ok';
}

step_channel($state['ch1']);
step_channel($state['ch2']);

file_put_contents($stateFile, json_encode($state, JSON_PRETTY_PRINT));
echo json_encode($state);

