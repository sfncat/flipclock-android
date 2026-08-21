package one.alynx.flipclock.weather;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Free, no-API-key weather provider backed by Open-Meteo.
 * <p>
 * See <a href="https://open-meteo.com/">open-meteo.com</a>.
 */
public final class OpenMeteoWeatherProvider implements WeatherProvider {
    private static final String GEOCODING_URL =
            "https://geocoding-api.open-meteo.com/v1/search";
    private static final String FORECAST_URL =
            "https://api.open-meteo.com/v1/forecast";
    private static final int REQUEST_TIMEOUT_MS = 15000;

    @Override
    public List<GeoLocation> searchLocations(String name, String language)
            throws WeatherException {
        if (name == null || name.trim().isEmpty()) {
            throw new WeatherException("Location name is empty.");
        }

        String encoded;
        try {
            encoded = URLEncoder.encode(name.trim(), StandardCharsets.UTF_8.name());
        } catch (IOException e) {
            throw new WeatherException("Failed to encode location name.", e);
        }

        String url = String.format(Locale.US,
                "%s?name=%s&count=10&language=%s&format=json",
                GEOCODING_URL, encoded,
                language != null ? language : "zh");

        JSONObject root = fetchJson(url);
        if (!root.has("results")) {
            throw new WeatherException("No locations found for \"" + name + "\".");
        }

        JSONArray results = root.optJSONArray("results");
        if (results == null || results.length() == 0) {
            throw new WeatherException("No locations found for \"" + name + "\".");
        }

        List<GeoLocation> locations = new ArrayList<>();
        for (int i = 0; i < results.length(); ++i) {
            JSONObject item = results.optJSONObject(i);
            if (item == null) {
                continue;
            }
            String locName = item.optString("name", name);
            double latitude = item.optDouble("latitude", Double.NaN);
            double longitude = item.optDouble("longitude", Double.NaN);
            if (Double.isNaN(latitude) || Double.isNaN(longitude)) {
                continue;
            }
            String country = item.optString("country", null);
            String admin1 = item.optString("admin1", null);
            locations.add(new GeoLocation(locName, latitude, longitude,
                    country, admin1));
        }

        if (locations.isEmpty()) {
            throw new WeatherException("No valid locations found for \"" + name + "\".");
        }
        return locations;
    }

    @Override
    public WeatherInfo fetchWeather(GeoLocation location) throws WeatherException {
        if (location == null) {
            throw new WeatherException("Location is null.");
        }

        String url = String.format(Locale.US,
                "%s?latitude=%.4f&longitude=%.4f&current_weather=true",
                FORECAST_URL, location.getLatitude(), location.getLongitude());

        JSONObject root = fetchJson(url);
        if (!root.has("current_weather")) {
            throw new WeatherException("Current weather data missing in response.");
        }

        JSONObject current = root.optJSONObject("current_weather");
        if (current == null) {
            throw new WeatherException("Current weather data missing in response.");
        }

        double temperature = current.optDouble("temperature", Double.NaN);
        int weatherCode = current.optInt("weathercode", -1);
        if (Double.isNaN(temperature) || weatherCode < 0) {
            throw new WeatherException("Failed to parse current weather fields.");
        }

        String description = getWeatherDescription(weatherCode);
        return new WeatherInfo(location, (int) Math.round(temperature),
                weatherCode, description, System.currentTimeMillis());
    }

    private JSONObject fetchJson(String urlString) throws WeatherException {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(urlString);
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(REQUEST_TIMEOUT_MS);
            connection.setReadTimeout(REQUEST_TIMEOUT_MS);
            connection.setRequestProperty("Accept", "application/json");
            connection.setRequestProperty("User-Agent", "FlipClock-Weather/1.0");

            int responseCode = connection.getResponseCode();
            if (responseCode < 200 || responseCode >= 300) {
                throw new WeatherException("HTTP error " + responseCode
                        + " for " + urlString);
            }

            InputStream inputStream = connection.getInputStream();
            String response = readAll(inputStream);
            return new JSONObject(response);
        } catch (IOException e) {
            throw new WeatherException("Network error while fetching " + urlString, e);
        } catch (JSONException e) {
            throw new WeatherException("Failed to parse JSON response.", e);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private String readAll(InputStream inputStream) throws IOException {
        BufferedReader reader = new BufferedReader(
                new InputStreamReader(inputStream, StandardCharsets.UTF_8));
        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {
            sb.append(line);
        }
        reader.close();
        return sb.toString();
    }

    /**
     * Map Open-Meteo WMO weather code to a short Chinese description.
     */
    public static String getWeatherDescription(int weatherCode) {
        switch (weatherCode) {
            case 0:
                return "晴";
            case 1:
                return "少云";
            case 2:
                return "多云";
            case 3:
                return "阴";
            case 45:
                return "雾";
            case 48:
                return "雾凇";
            case 51:
                return "毛毛雨";
            case 53:
                return "中雨";
            case 55:
                return "大雨";
            case 56:
                return "冻雨";
            case 57:
                return "冻雨";
            case 61:
                return "小雨";
            case 63:
                return "中雨";
            case 65:
                return "大雨";
            case 66:
                return "冻雨";
            case 67:
                return "冻雨";
            case 71:
                return "小雪";
            case 73:
                return "中雪";
            case 75:
                return "大雪";
            case 77:
                return "雪粒";
            case 80:
                return "阵雨";
            case 81:
                return "阵雨";
            case 82:
                return "暴雨";
            case 85:
                return "阵雪";
            case 86:
                return "阵雪";
            case 95:
                return "雷阵雨";
            case 96:
                return "雷暴伴冰雹";
            case 99:
                return "雷暴伴冰雹";
            default:
                return "未知";
        }
    }
}
