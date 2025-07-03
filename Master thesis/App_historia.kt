package com.example.master_thesis.ui.notifications
import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.bumptech.glide.Glide
import com.example.master_thesis.databinding.FragmentTripDetailsBinding
import com.google.firebase.database.*

/**
 * Fragment wyświetlający szczegóły trasy z gałęzi vehicle/trip na żywo:
 * - dwie kolumny: początek i koniec (lat, lng, altitude, time)
 * - pod mapą statyczna mapa z dwoma markerami Start (zielony) i End (czerwony)
 * Aktualizuje się w czasie rzeczywistym.
 */
class TripDetailsFragment : Fragment() {

    private var _binding: FragmentTripDetailsBinding? = null
    private val binding get() = _binding!!

    private lateinit var database: DatabaseReference
    private lateinit var tripListener: ValueEventListener

    private val googleMapsKey = "Your Key"

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentTripDetailsBinding.inflate(inflater, container, false)
        return binding.root
    }

    @SuppressLint("SetTextI18n")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        // Referencja do vehicle/trip
        database = FirebaseDatabase.getInstance().getReference("vehicle/trip")

        tripListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val beginSnap = snapshot.child("begin")
                val endSnap = snapshot.child("end")

                val bLat = beginSnap.child("lat").getValue(Double::class.java) ?: 0.0
                val bLng = beginSnap.child("lng").getValue(Double::class.java) ?: 0.0
                val bAlt = beginSnap.child("altitude").getValue(Double::class.java) ?: 0.0
                val bTime = beginSnap.child("time").getValue(String::class.java) ?: ""

                val eLat = endSnap.child("lat").getValue(Double::class.java) ?: 0.0
                val eLng = endSnap.child("lng").getValue(Double::class.java) ?: 0.0
                val eAlt = endSnap.child("altitude").getValue(Double::class.java) ?: 0.0
                val eTime = endSnap.child("time").getValue(String::class.java) ?: ""

                // Aktualizacja UI
                binding.textBeginLabel.text = "Początek"
                binding.textBeginLat.text = "Lat: ${"%.6f".format(bLat)}"
                binding.textBeginLng.text = "Lng: ${"%.6f".format(bLng)}"
                binding.textBeginAlt.text = "Alt: ${"%.1f".format(bAlt)} m"
                binding.textBeginTime.text = "Time: $bTime"

                binding.textEndLabel.text = "Koniec"
                binding.textEndLat.text = "Lat: ${"%.6f".format(eLat)}"
                binding.textEndLng.text = "Lng: ${"%.6f".format(eLng)}"
                binding.textEndAlt.text = "Alt: ${"%.1f".format(eAlt)} m"
                binding.textEndTime.text = "Time: $eTime"

                // Mapa
                val mapUrl = buildString {
                    append("https://maps.googleapis.com/maps/api/staticmap?size=600x300")
                    append("&markers=color:green%7Clabel:B%7C$bLat,$bLng")
                    append("&markers=color:red%7Clabel:E%7C$eLat,$eLng")
                    append("&key=$googleMapsKey")
                }
                Glide.with(this@TripDetailsFragment)
                    .load(mapUrl)
                    .into(binding.mapImageView)
            }

            override fun onCancelled(error: DatabaseError) {
                binding.textBeginLabel.text = "Błąd ładowania"
            }
        }
        database.addValueEventListener(tripListener)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        // Odłączamy listener, by nie wywoływał po zniszczeniu widoku
        database.removeEventListener(tripListener)
        _binding = null
    }
}