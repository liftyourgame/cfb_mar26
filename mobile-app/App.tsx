/**
 * humn log — BLE log monitor for the ESP32 captive portal.
 *
 * Scans for the "humn.au-log" BLE peripheral, connects to it,
 * subscribes to the log characteristic, reassembles 20-byte chunks
 * into complete messages, and displays a live scrolling log.
 */

import 'expo-dev-client';
import React, { useCallback, useEffect, useRef, useState } from 'react';
import {
  FlatList,
  Platform,
  SafeAreaView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { StatusBar } from 'expo-status-bar';
import { BleManager, Device, State } from 'react-native-ble-plx';

// ─── BLE constants ────────────────────────────────────────────────────────────

const DEVICE_NAME = 'humn.au-log';
const SERVICE_UUID = '91bad492-b950-4226-aa2b-4ede9fa42f59';
const CHAR_UUID = 'ca73b3ba-39f6-4ab3-91ae-186dc9577d99';

// ─── Types ────────────────────────────────────────────────────────────────────

type AppState = 'idle' | 'scanning' | 'found' | 'connecting' | 'connected' | 'error';

interface LogLine {
  id: string;
  timestamp: string;
  text: string;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

const bleManager = new BleManager();

/** Returns HH:MM:SS timestamp string */
function now(): string {
  return new Date().toTimeString().slice(0, 8);
}

/** Decode base64 BLE value to a UTF-8 string */
function decodeValue(b64: string): string {
  try {
    return atob(b64);
  } catch {
    return '';
  }
}

// ─── App ─────────────────────────────────────────────────────────────────────

export default function App() {
  const [appState, setAppState] = useState<AppState>('idle');
  const [statusText, setStatusText] = useState('Ready to scan');
  const [logs, setLogs] = useState<LogLine[]>([]);
  const [bleReady, setBleReady] = useState(false);

  const deviceRef = useRef<Device | null>(null);
  // Buffer for reassembling 20-byte BLE chunks into full log lines
  const chunkBuffer = useRef('');
  const listRef = useRef<FlatList<LogLine>>(null);
  const logCounter = useRef(0);

  // ── BLE state listener ──────────────────────────────────────────────────────
  useEffect(() => {
    const sub = bleManager.onStateChange((state) => {
      if (state === State.PoweredOn) {
        setBleReady(true);
      } else {
        setBleReady(false);
        if (state === State.PoweredOff) {
          setStatusText('Bluetooth is off — please enable it');
          setAppState('error');
        }
      }
    }, true);
    return () => sub.remove();
  }, []);

  // ── Cleanup on unmount ──────────────────────────────────────────────────────
  useEffect(() => {
    return () => {
      deviceRef.current?.cancelConnection().catch(() => {});
    };
  }, []);

  // ── Append a log line ───────────────────────────────────────────────────────
  const appendLog = useCallback((text: string) => {
    if (!text.trim()) return;
    setLogs((prev) => [
      ...prev,
      { id: String(logCounter.current++), timestamp: now(), text: text.trim() },
    ]);
    // Auto-scroll to bottom — slight delay so the new item is rendered first
    setTimeout(() => listRef.current?.scrollToEnd({ animated: true }), 50);
  }, []);

  // ── Handle incoming BLE notification chunk ──────────────────────────────────
  const handleChunk = useCallback(
    (b64: string) => {
      const chunk = decodeValue(b64);
      // The ESP32 sends messages terminated with \0 (null byte) or just ends
      // at the 20-byte MTU boundary. Accumulate and flush on null byte.
      for (const char of chunk) {
        if (char === '\0' || char === '\n') {
          appendLog(chunkBuffer.current);
          chunkBuffer.current = '';
        } else {
          chunkBuffer.current += char;
        }
      }
    },
    [appendLog]
  );

  // ── Scan ────────────────────────────────────────────────────────────────────
  const startScan = useCallback(() => {
    if (!bleReady) return;
    setAppState('scanning');
    setStatusText('Scanning for humn.au-log…');

    bleManager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        setStatusText(`Scan error: ${error.message}`);
        setAppState('error');
        return;
      }
      if (device?.name === DEVICE_NAME) {
        bleManager.stopDeviceScan();
        deviceRef.current = device;
        setAppState('found');
        setStatusText(`Found ${DEVICE_NAME} — tap Connect`);
      }
    });

    // Stop scanning after 10 s if not found
    setTimeout(() => {
      if (deviceRef.current) return;
      bleManager.stopDeviceScan();
      setAppState('idle');
      setStatusText('Device not found — try again');
    }, 10000);
  }, [bleReady]);

  // ── Connect ─────────────────────────────────────────────────────────────────
  const connect = useCallback(async () => {
    const device = deviceRef.current;
    if (!device) return;
    setAppState('connecting');
    setStatusText('Connecting…');
    try {
      const connected = await device.connect();
      const discovered = await connected.discoverAllServicesAndCharacteristics();
      deviceRef.current = discovered;

      setAppState('connected');
      setStatusText('Connected');
      appendLog('--- connected ---');

      // Subscribe to NOTIFY characteristic
      discovered.monitorCharacteristicForService(
        SERVICE_UUID,
        CHAR_UUID,
        (error, characteristic) => {
          if (error) {
            // Disconnection triggers this with a "cancelled" error — treat as normal
            if (!error.message?.includes('cancelled')) {
              appendLog(`[BLE error] ${error.message}`);
            }
            setAppState('idle');
            setStatusText('Disconnected');
            appendLog('--- disconnected ---');
            deviceRef.current = null;
            return;
          }
          if (characteristic?.value) {
            handleChunk(characteristic.value);
          }
        }
      );
    } catch (e: any) {
      setStatusText(`Connection failed: ${e.message}`);
      setAppState('error');
    }
  }, [appendLog, handleChunk]);

  // ── Disconnect ──────────────────────────────────────────────────────────────
  const disconnect = useCallback(async () => {
    await deviceRef.current?.cancelConnection().catch(() => {});
    deviceRef.current = null;
    setAppState('idle');
    setStatusText('Disconnected');
  }, []);

  // ── Clear logs ──────────────────────────────────────────────────────────────
  const clearLogs = useCallback(() => setLogs([]), []);

  // ─── Render ─────────────────────────────────────────────────────────────────

  const statusColor =
    appState === 'connected'
      ? COLORS.green
      : appState === 'error'
      ? COLORS.red
      : COLORS.muted;

  return (
    <SafeAreaView style={styles.root}>
      <StatusBar style="light" />

      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.title}>humn</Text>
        <View style={[styles.badge, { backgroundColor: statusColor + '22' }]}>
          <View style={[styles.badgeDot, { backgroundColor: statusColor }]} />
          <Text style={[styles.badgeText, { color: statusColor }]}>{statusText}</Text>
        </View>
      </View>

      {/* Log list */}
      <FlatList
        ref={listRef}
        data={logs}
        keyExtractor={(item) => item.id}
        style={styles.list}
        contentContainerStyle={styles.listContent}
        renderItem={({ item }) => (
          <View style={styles.logRow}>
            <Text style={styles.timestamp}>{item.timestamp}</Text>
            <Text style={styles.logText}>{item.text}</Text>
          </View>
        )}
        ListEmptyComponent={
          <Text style={styles.empty}>No log messages yet.</Text>
        }
      />

      {/* Footer controls */}
      <View style={styles.footer}>
        {appState === 'idle' || appState === 'error' ? (
          <Button label="Scan" onPress={startScan} disabled={!bleReady} />
        ) : appState === 'scanning' ? (
          <Button label="Scanning…" disabled />
        ) : appState === 'found' ? (
          <Button label="Connect" onPress={connect} accent />
        ) : appState === 'connecting' ? (
          <Button label="Connecting…" disabled />
        ) : (
          <Button label="Disconnect" onPress={disconnect} danger />
        )}
        {logs.length > 0 && (
          <Button label="Clear" onPress={clearLogs} muted />
        )}
      </View>
    </SafeAreaView>
  );
}

// ─── Button component ─────────────────────────────────────────────────────────

interface ButtonProps {
  label: string;
  onPress?: () => void;
  disabled?: boolean;
  accent?: boolean;
  danger?: boolean;
  muted?: boolean;
}

function Button({ label, onPress, disabled, accent, danger, muted }: ButtonProps) {
  const bg = danger
    ? COLORS.red + '22'
    : accent
    ? COLORS.green + '22'
    : muted
    ? 'transparent'
    : '#222';
  const border = danger
    ? COLORS.red
    : accent
    ? COLORS.green
    : muted
    ? '#333'
    : '#444';
  const color = danger
    ? COLORS.red
    : accent
    ? COLORS.green
    : muted
    ? COLORS.muted
    : '#fff';

  return (
    <TouchableOpacity
      style={[styles.button, { backgroundColor: bg, borderColor: border }, disabled && styles.buttonDisabled]}
      onPress={onPress}
      disabled={disabled}
      activeOpacity={0.7}
    >
      <Text style={[styles.buttonText, { color }, disabled && styles.buttonTextDisabled]}>
        {label}
      </Text>
    </TouchableOpacity>
  );
}

// ─── Styles ───────────────────────────────────────────────────────────────────

const COLORS = {
  bg: '#0a0a0a',
  surface: '#111',
  green: '#4caf50',
  red: '#e53935',
  muted: '#555',
  text: '#e0e0e0',
  dim: '#888',
  timestamp: '#444',
};

const styles = StyleSheet.create({
  root: {
    flex: 1,
    backgroundColor: COLORS.bg,
  },
  header: {
    paddingHorizontal: 20,
    paddingTop: 12,
    paddingBottom: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#1e1e1e',
    gap: 8,
  },
  title: {
    color: '#fff',
    fontSize: 24,
    fontWeight: '300',
    letterSpacing: 6,
  },
  badge: {
    flexDirection: 'row',
    alignItems: 'center',
    alignSelf: 'flex-start',
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 20,
    gap: 6,
  },
  badgeDot: {
    width: 6,
    height: 6,
    borderRadius: 3,
  },
  badgeText: {
    fontSize: 12,
    fontWeight: '400',
  },
  list: {
    flex: 1,
  },
  listContent: {
    paddingHorizontal: 16,
    paddingVertical: 12,
    gap: 4,
  },
  logRow: {
    flexDirection: 'row',
    gap: 10,
    alignItems: 'flex-start',
  },
  timestamp: {
    color: COLORS.timestamp,
    fontSize: 11,
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
    paddingTop: 2,
    minWidth: 60,
  },
  logText: {
    color: COLORS.text,
    fontSize: 13,
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
    flex: 1,
  },
  empty: {
    color: COLORS.muted,
    textAlign: 'center',
    marginTop: 60,
    fontSize: 14,
  },
  footer: {
    flexDirection: 'row',
    padding: 16,
    gap: 10,
    borderTopWidth: 1,
    borderTopColor: '#1e1e1e',
  },
  button: {
    flex: 1,
    paddingVertical: 14,
    borderRadius: 10,
    borderWidth: 1,
    alignItems: 'center',
  },
  buttonDisabled: {
    opacity: 0.4,
  },
  buttonText: {
    fontSize: 15,
    fontWeight: '500',
  },
  buttonTextDisabled: {
    color: COLORS.muted,
  },
});
