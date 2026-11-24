/* ============================================================================
   TRAZEN REAL-TIME DASHBOARD CONTROLLER
   ============================================================================ */

document.addEventListener("DOMContentLoaded", () => {
    // 1. Initialize Map
    initTrazenMap(13.0604, 80.2496);

    // 2. Start Live Polling Loop
    setInterval(pollLiveStatus, 1500);
});

async function pollLiveStatus() {
    try {
        // Fetch Live Telemetry & Latest Frame
        const telRes = await fetch("/api/v1/telemetry/live");
        if (telRes.ok) {
            const data = await telRes.json();
            
            // Update Vehicle Marker on Map
            if (data.latest) {
                updateVehicleLocation(
                    data.latest.latitude,
                    data.latest.longitude,
                    data.latest.speed_kmh,
                    data.latest.heading_deg
                );
            }

            // Update Live Stream Preview
            if (data.latest_frame && data.latest_frame.url) {
                const imgElem = document.getElementById("liveStreamImg");
                if (imgElem) {
                    imgElem.src = data.latest_frame.url + "?t=" + new Date().getTime();
                }

                const hudElem = document.getElementById("hudInfo");
                if (hudElem) {
                    hudElem.textContent = `REC ${data.latest_frame.timestamp} | LAT ${data.latest_frame.lat.toFixed(4)} LON ${data.latest_frame.lon.toFixed(4)}`;
                }

                const speedElem = document.getElementById("hudSpeed");
                if (speedElem) {
                    speedElem.textContent = `${data.latest_frame.speed.toFixed(1)} KM/H`;
                }
            }
        }

        // Fetch Live Stats
        const statsRes = await fetch("/api/v1/stats");
        if (statsRes.ok) {
            const stats = await statsRes.json();
            document.getElementById("statTotalFrames").textContent = stats.total_frames || 0;
            document.getElementById("statTotalViolations").textContent = stats.total_violations || 0;
            document.getElementById("statPendingReview").textContent = stats.pending_review || 0;
            document.getElementById("statActiveNodes").textContent = stats.active_devices || 1;
        }

    } catch (err) {
        console.warn("[TRAZEN] Polling heartbeat error:", err);
    }
}
