package one.alynx.flipclock.weather;

/**
 * Current weather information for a specific location.
 */
public final class WeatherInfo {
    private final GeoLocation location;
    private final int temperature;
    private final int weatherCode;
    private final String description;
    private final long timestamp;

    public WeatherInfo(GeoLocation location, int temperature, int weatherCode,
                       String description, long timestamp) {
        this.location = location;
        this.temperature = temperature;
        this.weatherCode = weatherCode;
        this.description = description;
        this.timestamp = timestamp;
    }

    public GeoLocation getLocation() {
        return location;
    }

    public int getTemperature() {
        return temperature;
    }

    public int getWeatherCode() {
        return weatherCode;
    }

    public String getDescription() {
        return description;
    }

    public long getTimestamp() {
        return timestamp;
    }

    @Override
    public String toString() {
        return "WeatherInfo{"
                + "location=" + location
                + ", temperature=" + temperature
                + ", weatherCode=" + weatherCode
                + ", description='" + description + '\''
                + ", timestamp=" + timestamp
                + '}';
    }
}
