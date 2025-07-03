package com.example.master_thesis.ui.dashboard
import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.bumptech.glide.Glide
import com.example.master_thesis.databinding.FragmentDashboardBinding
import com.google.firebase.database.*

class DashboardFragment : Fragment() {

    private var _binding: FragmentDashboardBinding? = null
    private val binding get() = _binding!!

    private lateinit var database: DatabaseReference

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentDashboardBinding.inflate(inflater, container, false)
        val root = binding.root

        // Inicjalizacja Firebase
        database = FirebaseDatabase.getInstance().reference
        val gpsRef = database.child("vehicle").child("currentLocation")

        gpsRef.addValueEventListener(object : ValueEventListener {
            @SuppressLint("SetTextI18n")
            override fun onDataChange(snapshot: DataSnapshot) {
                val lat = snapshot.child("lat").getValue(Double::class.java) ?: return
                val lng = snapshot.child("lng").getValue(Double::class.java) ?: return
                val time = snapshot.child("time").getValue(String::class.java) ?: "brak"
                val speed = snapshot.child("speed").getValue(Double::class.java) ?: 0.0
                val altitude = snapshot.child("altitude").getValue(Double::class.java) ?: 0.0

                binding.textLat.text = "Lat: $lat"
                binding.textLng.text = "Lng: $lng"
                binding.textTime.text = "Time: $time"
                binding.textSpeed.text = "Speed: %.2f km/h".format(speed)
                binding.textAlt.text = "Altitude: %.2f m".format(altitude)

                val mapUrl = "https://maps.googleapis.com/maps/api/staticmap" +
                        "?center=$lat,$lng + " +
                        "&zoom=15&size=600x300" +
                        "&markers=color:red%7C$lat,$lng" +
                        "&kYour Key"

                Glide.with(this@DashboardFragment)
                    .load(mapUrl)
                    .into(binding.mapPreview)

                binding.loadingIndicator.visibility = View.GONE

            }

            override fun onCancelled(error: DatabaseError) {
            }
        })

        return root
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}