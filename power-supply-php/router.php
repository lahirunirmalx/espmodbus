<?php
// Dev server router for: php -S localhost:8000 router.php
$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);

if ($path === '/getStatus') {
  require __DIR__ . '/api.php';
  return;
}

// Let PHP's built-in server serve existing files in /public
$full = __DIR__ . '/public' . $path;
if ($path !== '/' && file_exists($full) && !is_dir($full)) {
  return false;
}

// Default to index.html
require __DIR__ . '/public/index.html';
