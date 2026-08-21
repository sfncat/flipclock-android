package one.alynx.flipclock.weather;

/**
 * Represents a geographic location matched from a user-supplied name.
 */
public final class GeoLocation {
    private final String name;
    private final double latitude;
    private final double longitude;
    private final String country;
    private final String admin1;

    public GeoLocation(String name, double latitude, double longitude,
                       String country, String admin1) {
        this.name = name;
        this.latitude = latitude;
        this.longitude = longitude;
        this.country = country;
        this.admin1 = admin1;
    }

    public String getName() {
        return name;
    }

    public double getLatitude() {
        return latitude;
    }

    public double getLongitude() {
        return longitude;
    }

    public String getCountry() {
        return country;
    }

    public String getAdmin1() {
        return admin1;
    }

    @Override
    public String toString() {
        return "GeoLocation{"
                + "name='" + name + '\''
                + ", latitude=" + latitude
                + ", longitude=" + longitude
                + ", country='" + country + '\''
                + ", admin1='" + admin1 + '\''
                + '}';
    }
}
