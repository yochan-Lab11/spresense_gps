// TODO:
// BLE デバイスのサービスUUIDとキャラクタリスティックUUIDの
// 設定をお願いします。
const BLUETOOTH_SERVICE_UUID = "00003802-0000-1000-8000-00805f9b34fb";
const BLUETOOTH_CHARACTERISTIC_UUID = "00004a02-0000-1000-8000-00805f9b34fb";

let globalMap = null;
let globalCharacteristic = null;

/* ---------- Debug Code ---------- */
const _console = console;

const writeLog = (message) => {
  const ul = document.getElementById("log");
  const li = document.createElement("li");
  li.appendChild(document.createTextNode(message));
  ul.appendChild(li);
};

console = {
  error: (message) => { writeLog('[ERROR] ' + message) }, 
  log: (message) => { writeLog('[LOG] ' + message) }, 
};

/* ------------------------------ */

function getCurrentPosition(options) {
  return new Promise((resolve, reject) => 
    navigator.geolocation.getCurrentPosition(resolve, reject, options)
  );
}

// Initialize Google map
async function initMap() {
  const options = {
    enableHighAccuracy: true,
    timeout: 5000,
    maximumAge: 0,
  };

  // NOTE:
  // 現在地が取得できない場合のフォールバック値
  let lat = 35.730629;
  let lng = 139.708697;

  try {
    const position = await getCurrentPosition(options);
    lat = position.coords.latitude;
    lng = position.coords.longitude;
  } catch(error) {
    console.error('Failed to get current position' + error);
  }

  const mapLatLng = new google.maps.LatLng(lat, lng);
  // Create a new map centered at the provided latitude and longitude
  globalMap = new google.maps.Map(document.getElementById('map'), {
    center: mapLatLng,
    zoom: 15
  });

  // NOTE:
  // 初期表示時に現在地にマーカーをセットするには
  // 以下のコメントアウトを外します
  // addMarker(lat, lng, 'current position');
}

function addMarker(lat, lng, title) {
  // Add a marker for the provided location
  const mapLatLng = new google.maps.LatLng(lat, lng);
  const marker = new google.maps.Marker({
    position: mapLatLng,
    map: globalMap,
    title: title,
  });
}

function parseBluetoothData(byteArray) {
  const decoder = new TextDecoder("utf-8");
  const str = decoder.decode(byteArray);
  console.log('Decoded Bluetooth Data: ' + str);

  const [latitude, longitude] = str.split(',');
  return {
    latitude: parseFloat(latitude), 
    longitude: parseFloat(longitude),
  };
}

// Function to handle Bluetooth notifications
function handleBluetoothNotifications(event) {
  const value = event.target.value;
  const { latitude, longitude } = parseBluetoothData(value);
  console.log('Latitude:' + latitude + 'Longitude:' + longitude);

  addMarker(latitude, longitude, 'Bluetooth Device position');
}

// Request Bluetooth device
async function requestBluetoothDevice() {
  try {
    const device = await navigator.bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [BLUETOOTH_SERVICE_UUID]
    });

    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(BLUETOOTH_SERVICE_UUID);
    globalCharacteristic = await service.getCharacteristic(BLUETOOTH_CHARACTERISTIC_UUID);

    // Start notifications
    await globalCharacteristic.startNotifications();
    globalCharacteristic.addEventListener(
      'characteristicvaluechanged', 
      handleBluetoothNotifications
    );
  } catch (error) {
    console.error('Error requesting Bluetooth device: ' + error);
  }
}

async function onStartButtonClick() {
  await requestBluetoothDevice();
}

async function onStopButtonClick() {
  if (globalCharacteristic) {
    try {
      await globalCharacteristic.stopNotifications();
      console.log('> Notifications stopped');
      globalCharacteristic.removeEventListener(
        'characteristicvaluechanged',
        handleNotifications
      );
    } catch(error) {
      console.error('Failed to stop notifications: ' + error);
    }
  }
}
