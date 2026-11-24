/* ============================================================================
   TRAZEN LEAFLET.JS GPS TRACKING ENGINE
   ============================================================================ */

let trazenMap = null;
let vehicleMarker = null;
let trailPolyline = null;
let trailCoordinates = [];
let violationMarkersLayer = null;

function initTrazenMap(defaultLat = 13.0604, defaultLon = 80.2496) {
    const mapElement = document.getElementById("map");
    if (!mapElement) return;

    // Initialize Map centered around default coordinates (Chennai / LICET)
    trazenMap = L.map("map", {
        center: [defaultLat, defaultLon],
        zoom: 15,
        zoomControl: true
    });

    // Dark Tile Layer (CartoDB Dark Matter / OSM)
    L.tileLayer("https://{s}.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}{r}.png", {
        attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attributions">CARTO</a>',
        subdomains: "abcd",
        maxZoom: 19
    }).addTo(trazenMap);

    // Layer for violation incident pins
    violationMarkersLayer = L.layerGroup().addTo(trazenMap);

    // Vehicle Marker Icon
    const vehicleIcon = L.divIcon({
        className: 'vehicle-marker-icon',
        html: `<div style="background:#3b82f6;width:18px;height:18px;border-radius:50%;border:3px solid #fff;box-shadow:0 0 12px #3b82f6;"></div>`,
        iconSize: [18, 18],
        iconAnchor: [9, 9]
    });

    vehicleMarker = L.marker([defaultLat, defaultLon], { icon: vehicleIcon }).addTo(trazenMap);
    vehicleMarker.bindPopup("<strong>TRAZEN NODE-01</strong><br>Status: Active");

    // Trajectory Polyline
    trailPolyline = L.polyline([], {
        color: '#06b6d4',
        weight: 4,
        opacity: 0.85,
        dashArray: '6, 8'
    }).addTo(trazenMap);
}

function updateVehicleLocation(lat, lon, speed, heading) {
    if (!trazenMap || !vehicleMarker) return;

    const newLatLng = [lat, lon];
    vehicleMarker.setLatLng(newLatLng);
    vehicleMarker.getPopup().setContent(
        `<strong>TRAZEN NODE-01</strong><br>Speed: ${speed.toFixed(1)} km/h<br>Coordinates: ${lat.toFixed(5)}, ${lon.toFixed(5)}`
    );

    // Append to breadcrumb trail
    trailCoordinates.push(newLatLng);
    if (trailCoordinates.length > 50) {
        trailCoordinates.shift();
    }
    trailPolyline.setLatLngs(trailCoordinates);
}

function addViolationPin(lat, lon, type, id, time) {
    if (!violationMarkersLayer) return;

    const violationIcon = L.divIcon({
        className: 'violation-marker-icon',
        html: `<div style="background:#f43f5e;width:16px;height:16px;border-radius:50%;border:2px solid #fff;box-shadow:0 0 10px #f43f5e;"></div>`,
        iconSize: [16, 16],
        iconAnchor: [8, 8]
    });

    const marker = L.marker([lat, lon], { icon: violationIcon }).addTo(violationMarkersLayer);
    marker.bindPopup(`
        <div style="font-family:sans-serif;font-size:12px;">
            <strong style="color:#e11d48;">VIOLATION FLAGGED</strong><br>
            <b>Type:</b> ${type}<br>
            <b>Time:</b> ${time}<br>
            <a href="/challan/${id}" target="_blank" style="color:#2563eb;text-decoration:none;font-weight:bold;">View E-Challan</a>
        </div>
    `);
}
