package com.stackof.flipclockv2.weather;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * Manages periodic weather updates for a user-specified location.
 * <p>
 * This class does not use the device location service; the caller supplies
 * a location name which is geocoded once and then cached.
 * <p>
 * All network work is performed on an internal background thread. Listener
 * callbacks are invoked on that same background thread.
 */
public final class WeatherManager {
    private static final String DEFAULT_PREFS_NAME = "flipclock_weather";
    private static final String PREFS_KEY_NAME = "weather_location_name";
    private static final String PREFS_KEY_LAT = "weather_latitude";
    private static final String PREFS_KEY_LON = "weather_longitude";
    private static final String PREFS_KEY_COUNTRY = "weather_country";
    private static final String PREFS_KEY_ADMIN1 = "weather_admin1";

    private static final int MIN_INTERVAL_HOURS = 1;
    private static final int MAX_INTERVAL_HOURS = 24;

    private final Context applicationContext;
    private final SharedPreferences prefs;
    private final WeatherProvider provider;
    private final Object lock = new Object();

    private ScheduledExecutorService executor;
    private ScheduledFuture<?> scheduledTask;
    private volatile boolean running = false;

    private int updateIntervalHours = 3;
    private WeatherListener listener;
    private GeoLocation lastLocation;
    private WeatherInfo lastInfo;

    /**
     * Create a WeatherManager using the default SharedPreferences name.
     */
    public WeatherManager(Context context) {
        this(context, DEFAULT_PREFS_NAME);
    }

    /**
     * Create a WeatherManager using a custom SharedPreferences name.
     *
     * @param context Application or activity context.
     * @param prefsName Name of the SharedPreferences used to cache location.
     */
    public WeatherManager(Context context, String prefsName) {
        if (context == null) {
            throw new IllegalArgumentException("Context is null.");
        }
        this.applicationContext = context.getApplicationContext();
        this.prefs = applicationContext.getSharedPreferences(
                prefsName != null ? prefsName : DEFAULT_PREFS_NAME,
                Context.MODE_PRIVATE);
        this.provider = new OpenMeteoWeatherProvider();
        this.lastLocation = loadCachedLocation();
    }

    /**
     * Set how often the weather should be refreshed.
     *
     * @param hours Interval in hours, clamped to 1–24.
     */
    public void setUpdateIntervalHours(int hours) {
        if (hours < MIN_INTERVAL_HOURS) {
            hours = MIN_INTERVAL_HOURS;
        } else if (hours > MAX_INTERVAL_HOURS) {
            hours = MAX_INTERVAL_HOURS;
        }
        synchronized (lock) {
            this.updateIntervalHours = hours;
            if (running) {
                scheduleNextFetch();
            }
        }
    }

    public int getUpdateIntervalHours() {
        synchronized (lock) {
            return updateIntervalHours;
        }
    }

    /**
     * Set a listener to receive weather updates and errors.
     */
    public void setWeatherListener(WeatherListener listener) {
        synchronized (lock) {
            this.listener = listener;
        }
    }

    /**
     * Geocode the supplied location name, save the first matched location,
     * fetch current weather, and schedule periodic updates if running.
     */
    public void setLocation(final String name) {
        if (name == null || name.trim().isEmpty()) {
            notifyError(new WeatherException("Location name is empty."));
            return;
        }

        submit(new Runnable() {
            @Override
            public void run() {
                try {
                    List<GeoLocation> results = provider.searchLocations(
                            name.trim(), "zh");
                    if (results.isEmpty()) {
                        throw new WeatherException(
                                "No locations found for \"" + name + "\".");
                    }
                    GeoLocation chosen = results.get(0);
                    saveLocation(chosen);
                    synchronized (lock) {
                        lastLocation = chosen;
                    }
                    fetchAndNotify(chosen);
                    synchronized (lock) {
                        if (running) {
                            scheduleNextFetch();
                        }
                    }
                } catch (Exception e) {
                    notifyError(e);
                }
            }
        });
    }

    /**
     * Use an already resolved location directly.
     */
    public void setLocation(final GeoLocation location) {
        if (location == null) {
            notifyError(new WeatherException("Location is null."));
            return;
        }

        saveLocation(location);
        synchronized (lock) {
            lastLocation = location;
        }

        submit(new Runnable() {
            @Override
            public void run() {
                try {
                    fetchAndNotify(location);
                    synchronized (lock) {
                        if (running) {
                            scheduleNextFetch();
                        }
                    }
                } catch (Exception e) {
                    notifyError(e);
                }
            }
        });
    }

    /**
     * Fetch weather immediately using the last known location.
     */
    public void refreshNow() {
        final GeoLocation location;
        synchronized (lock) {
            location = lastLocation;
        }
        if (location == null) {
            notifyError(new WeatherException("No location set."));
            return;
        }

        submit(new Runnable() {
            @Override
            public void run() {
                try {
                    fetchAndNotify(location);
                } catch (Exception e) {
                    notifyError(e);
                }
            }
        });
    }

    /**
     * Start periodic weather updates. If a location is already known,
     * a refresh is performed immediately.
     */
    public void start() {
        synchronized (lock) {
            if (running) {
                return;
            }
            running = true;
            if (executor == null || executor.isShutdown()) {
                executor = Executors.newSingleThreadScheduledExecutor();
            }
            if (lastLocation != null) {
                final GeoLocation location = lastLocation;
                executor.submit(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            fetchAndNotify(location);
                            synchronized (lock) {
                                if (running) {
                                    scheduleNextFetch();
                                }
                            }
                        } catch (Exception e) {
                            notifyError(e);
                            synchronized (lock) {
                                if (running) {
                                    scheduleNextFetch();
                                }
                            }
                        }
                    }
                });
            }
        }
    }

    /**
     * Stop periodic weather updates. Pending network requests may still complete.
     */
    public void stop() {
        synchronized (lock) {
            running = false;
            if (scheduledTask != null) {
                scheduledTask.cancel(false);
                scheduledTask = null;
            }
            if (executor != null) {
                executor.shutdownNow();
                executor = null;
            }
        }
    }

    /**
     * @return The most recently fetched weather, or null if none.
     */
    public WeatherInfo getLastWeatherInfo() {
        synchronized (lock) {
            return lastInfo;
        }
    }

    /**
     * @return The last resolved location, or null if none.
     */
    public GeoLocation getLastLocation() {
        synchronized (lock) {
            return lastLocation;
        }
    }

    private void fetchAndNotify(GeoLocation location) throws WeatherException {
        WeatherInfo info = provider.fetchWeather(location);
        synchronized (lock) {
            lastInfo = info;
        }
        notifyUpdate(info);
    }

    private void scheduleNextFetch() {
        synchronized (lock) {
            if (scheduledTask != null) {
                scheduledTask.cancel(false);
            }
            if (executor == null || executor.isShutdown()) {
                return;
            }
            final GeoLocation location = lastLocation;
            if (location == null) {
                return;
            }
            long delayHours = updateIntervalHours;
            scheduledTask = executor.scheduleAtFixedRate(new Runnable() {
                @Override
                public void run() {
                    try {
                        fetchAndNotify(location);
                    } catch (Exception e) {
                        notifyError(e);
                    }
                }
            }, delayHours, delayHours, TimeUnit.HOURS);
        }
    }

    private void submit(Runnable runnable) {
        synchronized (lock) {
            if (executor == null || executor.isShutdown()) {
                executor = Executors.newSingleThreadScheduledExecutor();
            }
            executor.submit(runnable);
        }
    }

    private void saveLocation(GeoLocation location) {
        SharedPreferences.Editor editor = prefs.edit();
        editor.putString(PREFS_KEY_NAME, location.getName());
        editor.putLong(PREFS_KEY_LAT, Double.doubleToRawLongBits(location.getLatitude()));
        editor.putLong(PREFS_KEY_LON, Double.doubleToRawLongBits(location.getLongitude()));
        if (location.getCountry() != null) {
            editor.putString(PREFS_KEY_COUNTRY, location.getCountry());
        } else {
            editor.remove(PREFS_KEY_COUNTRY);
        }
        if (location.getAdmin1() != null) {
            editor.putString(PREFS_KEY_ADMIN1, location.getAdmin1());
        } else {
            editor.remove(PREFS_KEY_ADMIN1);
        }
        editor.apply();
    }

    private GeoLocation loadCachedLocation() {
        String name = prefs.getString(PREFS_KEY_NAME, null);
        if (name == null) {
            return null;
        }
        if (!prefs.contains(PREFS_KEY_LAT) || !prefs.contains(PREFS_KEY_LON)) {
            return null;
        }
        double latitude = Double.longBitsToDouble(prefs.getLong(PREFS_KEY_LAT, 0));
        double longitude = Double.longBitsToDouble(prefs.getLong(PREFS_KEY_LON, 0));
        String country = prefs.getString(PREFS_KEY_COUNTRY, null);
        String admin1 = prefs.getString(PREFS_KEY_ADMIN1, null);
        return new GeoLocation(name, latitude, longitude, country, admin1);
    }

    private void notifyUpdate(WeatherInfo info) {
        WeatherListener callback;
        synchronized (lock) {
            callback = listener;
        }
        if (callback != null) {
            callback.onWeatherUpdated(info);
        }
    }

    private void notifyError(Throwable error) {
        WeatherListener callback;
        synchronized (lock) {
            callback = listener;
        }
        if (callback != null) {
            callback.onError(error);
        }
    }
}
