package com.example.master_thesis.ui.home

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.os.Build
import android.os.Bundle
import android.text.SpannableString
import android.text.style.ForegroundColorSpan
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.app.NotificationCompat
import androidx.core.net.toUri
import androidx.fragment.app.Fragment
import com.example.master_thesis.R
import com.example.master_thesis.databinding.FragmentHomeBinding
import com.google.firebase.database.*
import com.squareup.picasso.Picasso

class HomeFragment : Fragment() {

    private var _binding: FragmentHomeBinding? = null
    private val binding get() = _binding!!

    private lateinit var database: DatabaseReference
    private lateinit var vehicleListener: ValueEventListener

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentHomeBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Inicjalizacja RTDB
        database = FirebaseDatabase.getInstance().reference
        val vehicleRef = database.child("vehicle")

        // Zapisujemy listener i podpinamy go
        vehicleListener = object : ValueEventListener {
            @SuppressLint("SetTextI18n")
            override fun onDataChange(snapshot: DataSnapshot) {
                // --- LAST PARKING ---
                snapshot.child("lastParking").let { parkSnap ->
                    val lat = parkSnap.child("lat").getValue(Double::class.java) ?: return@let
                    val lng = parkSnap.child("lng").getValue(Double::class.java) ?: return@let
                    val time = parkSnap.child("time").getValue(String::class.java) ?: ""

                    // Budujemy URL do Static Maps API
                    val mapUrl = "https://maps.googleapis.com/maps/api/staticmap" +
                            "?center=$lat,$lng + " +
                            "&zoom=15&size=600x300" +
                            "&markers=color:red%7C$lat,$lng" +
                            "&Your key"

                    // Wczytujemy mapę
                    Picasso.get()
                        .load(mapUrl)
                        .into(binding.mapPreview)

                    // Wyświetlamy czas ostatniego parkowania pod mapą
                    binding.textParkingTime.visibility = View.VISIBLE
                    binding.textParkingTime.text = "Czas parkowania: $time"

                    // Kliknięcie otwiera mapę w Google Maps
                    binding.mapPreview.setOnClickListener {
                        val uri = "geo:$lat,$lng?q=$lat,$lng(Miejsce+zaparkowania)".toUri()
                        Intent(Intent.ACTION_VIEW, uri).apply {
                            setPackage("com.google.android.apps.maps")
                            if (resolveActivity(requireActivity().packageManager) != null) {
                                startActivity(this)
                            }
                        }
                    }
                }

                // --- CURRENT STATUS ---
                snapshot.child("currentStatus").let { statSnap ->
                    val isMoving = statSnap.child("isMoving").getValue(Boolean::class.java) ?: false
                    val code = statSnap.child("status").getValue(Int::class.java) ?: -1

                    // Wybór tekstu i stylu
                    val statusSpannable: SpannableString = when {
                        isMoving && code == 1 -> styledText("URUCHOMIONY", color = Color.GREEN)
                        !isMoving && code == 0 -> styledText("ZGASZONY", bold = true)
                        !isMoving && code == 2 -> styledText("KRADZIEŻ", color = Color.RED, bold = true)
                        else -> SpannableString("NIEZNANY")
                    }

                    // Łączenie z etykietą
                    val label = SpannableString("Aktualny status pojazdu: ")
                    val combined = SpannableString(label.toString() + statusSpannable.toString())
                    statusSpannable.getSpans(0, statusSpannable.length, Any::class.java).forEach { span ->
                        combined.setSpan(
                            span,
                            label.length,
                            label.length + statusSpannable.length,
                            SpannableString.SPAN_EXCLUSIVE_EXCLUSIVE
                        )
                    }

                    binding.textHome.text = combined
                    showVehicleStatusNotification(requireContext(), combined.toString())
                }
            }

            override fun onCancelled(error: DatabaseError) {
                // Opcjonalna obsługa błędów
            }
        }
        vehicleRef.addValueEventListener(vehicleListener)

        // Uprawnienia do notyfikacji (Android 13+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            requestPermissions(
                arrayOf(android.Manifest.permission.POST_NOTIFICATIONS),
                1001
            )
        }
    }

    override fun onDestroyView() {
        // Odłącz listener zanim binding zostanie zniszczony
        database.child("vehicle").removeEventListener(vehicleListener)
        _binding = null
        super.onDestroyView()
    }

    private fun showVehicleStatusNotification(context: Context, status: String) {
        val channelId = "vehicle_status_channel"
        val channelName = "Status pojazdu"
        val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as android.app.NotificationManager

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val ch = android.app.NotificationChannel(channelId, channelName, android.app.NotificationManager.IMPORTANCE_LOW)
            nm.createNotificationChannel(ch)
        }

        val notif = NotificationCompat.Builder(context, channelId)
            .setContentTitle("Status pojazdu")
            .setContentText(status)
            .setSmallIcon(R.drawable.notification_logo)
            .setOngoing(true)
            .setAutoCancel(false)
            .build()

        nm.notify(1, notif)
    }

    // Helper do tworzenia SpannableString
    private fun styledText(text: String, color: Int? = null, bold: Boolean = false): SpannableString {
        val span = SpannableString(text)
        if (bold) span.setSpan(StyleSpan(Typeface.BOLD), 0, text.length, SpannableString.SPAN_EXCLUSIVE_EXCLUSIVE)
        if (color != null) span.setSpan(ForegroundColorSpan(color), 0, text.length, SpannableString.SPAN_EXCLUSIVE_EXCLUSIVE)
        return span
    }
}